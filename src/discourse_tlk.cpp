// Discourse for U4 Tlk

#include <set>
#include "game.h"
#include "party.h"
#include "screen.h"
#include "settings.h"
#include "utils.h"

enum DialogueString {
    DS_NAME,
    DS_PRONOUN,
    DS_LOOK,
    DS_KEYWORD,
    DS_QUESTION,
    DS_ANSWER_Y,
    DS_ANSWER_N,
    // Used by the SMS style topic menu; these have no side effects and
    // return NULL if the dialogue has no such string.
    DS_TOPIC1,
    DS_TOPIC2,
    DS_TEXT_JOB,
    DS_TEXT_HEALTH,
    DS_TEXT_R1,
    DS_TEXT_R2,
    DS_TEXT_QUESTION
};

enum TalkOpcode {
    OP_NOP,
    OP_PAUSE_ASK = DS_QUESTION
};

enum VoicePart {
    VP_I_AM,
    VP_BYE,
    VP_GIVE,
    VP_JOIN,
    VP_KEYWORD
};

struct TalkState {
    Person* person;
    int16_t turnAway;
    int16_t nextOp;
    uint16_t voiceStream;
    uint16_t startVoiceLn;
    uint16_t askVoiceLn;
};

typedef const char* (*TalkFunc)(TalkState*, int, const char*);

//---------------------------------------------------------------------------
// SMS style conversation menu.
//
// Instead of typing, the player picks a topic from a list.  The two NPC
// keywords are only listed once unlocked.  The DOS data stores keywords as
// 4 letter stubs, so the full word is taken from the text that mentions it:
//
//  - If one of the NPC's own replies contains the word, the keyword is
//    unlocked once that reply has been heard in this conversation.
//  - Otherwise the word comes from other NPCs (a hint chain) and the keyword
//    is unlocked once the word has been heard from anyone.  These words are
//    remembered across conversations and saved with the game.
//  - A few stubs are not the start of any word; those use a fixed table.

static std::set<std::string> talkHeardWords;     // Saved with the game.

struct SmsTalk {
    std::set<std::string> heard;    // Words heard in this conversation.
    std::string word[2];            // Display word for topic 1 & 2.
    bool own[2];                    // Word found in the NPC's own text.
    bool asked[2];
    bool askedJob;
    int lastPick;
};

static bool smsInterface() {
    return xu4.settings->enhancementsOptions.smsInterface;
}

static void talkHeard(SmsTalk* st, const char* text)
{
    if (! text)
        return;
    std::string w;
    for (const char* cp = text; ; ++cp) {
        if (isalpha((unsigned char) *cp)) {
            w += tolower((unsigned char) *cp);
        } else {
            if (w.size() > 1) {
                talkHeardWords.insert(w);
                if (st)
                    st->heard.insert(w);
            }
            w.clear();
            if (! *cp)
                break;
        }
    }
}

// Stubs which are not a prefix of any word in the DOS dialogue.
static const char* smsFixedWord(const char* stub)
{
    static const char* table[] = {
        "ho e", "HO EYO",
        "nowh", "NOWHERE",
        "hide", "HIDING",
        "pris", "PRISONER",
        "beh.", "BEH...",
        "sine", "SINNED",
        NULL
    };
    for (const char** it = table; *it; it += 2) {
        if (strcasecmp(it[0], stub) == 0)
            return it[1];
    }
    return NULL;
}

// Find the first word in text starting with stub; return it upper-cased.
static std::string findWord(const char* text, const char* stub)
{
    size_t slen = strlen(stub);
    if (! text || ! slen)
        return std::string();
    for (const char* cp = text; *cp; ++cp) {
        if (isalpha((unsigned char) *cp) &&
            (cp == text || ! isalpha((unsigned char) cp[-1])) &&
            strncasecmp(cp, stub, slen) == 0) {
            std::string w;
            while (isalpha((unsigned char) *cp))
                w += toupper((unsigned char) *cp++);
            return w;
        }
    }
    return std::string();
}

static bool heardPrefix(const std::set<std::string>& words, const char* stub,
                        std::string* found)
{
    std::string s(stub);
    for (size_t i = 0; i < s.size(); ++i)
        s[i] = tolower((unsigned char) s[i]);
    std::set<std::string>::const_iterator it = words.lower_bound(s);
    if (it != words.end() && it->compare(0, s.size(), s) == 0) {
        if (found) {
            *found = *it;
            for (size_t i = 0; i < found->size(); ++i)
                (*found)[i] = toupper((unsigned char) (*found)[i]);
        }
        return true;
    }
    return false;
}

static void smsInit(SmsTalk* st, TalkFunc func, TalkState* ts)
{
    // Where to look for each keyword, in the order the SMS usually
    // unlocks them (keyword 1 after job, keyword 2 after keyword 1).
    static const int order[2][6] = {
        { DS_TEXT_JOB, DS_TEXT_HEALTH, DS_LOOK, DS_TEXT_R2, DS_TEXT_QUESTION, -1 },
        { DS_TEXT_R1, DS_TEXT_JOB, DS_TEXT_HEALTH, DS_LOOK, DS_TEXT_QUESTION, -1 }
    };

    st->askedJob = false;
    st->lastPick = 1;
    for (int k = 0; k < 2; ++k) {
        st->asked[k] = false;
        st->own[k] = false;
        st->word[k].clear();

        const char* stub = func(ts, DS_TOPIC1 + k, NULL);
        if (! stub || ! stub[0])
            continue;
        const char* fixed = smsFixedWord(stub);
        if (fixed) {
            st->word[k] = fixed;
            continue;
        }
        for (const int* f = order[k]; *f >= 0; ++f) {
            st->word[k] = findWord(func(ts, *f, NULL), stub);
            if (! st->word[k].empty()) {
                st->own[k] = true;
                break;
            }
        }
    }
}

static bool smsTopicUnlocked(const SmsTalk* st, TalkFunc func, TalkState* ts,
                             int k, std::string* display)
{
    const char* stub = func(ts, DS_TOPIC1 + k, NULL);
    if (! stub || ! stub[0])
        return false;
    if (smsFixedWord(stub)) {
        *display = st->word[k];
        return k ? st->asked[0] : st->askedJob;
    }
    if (st->own[k]) {
        *display = st->word[k];
        return heardPrefix(st->heard, stub, NULL);
    }
    return heardPrefix(talkHeardWords, stub, display);
}

/*
 * Show the topic menu and return the input string the DOS logic expects,
 * or an empty string for Bye.
 */
static std::string smsChooseTopic(SmsTalk* st, TalkFunc func, TalkState* ts)
{
    // Same order as the SMS talk menu.
    static const char* fixedTopics[] = {
        "Bye", "Name", "Look", "Job", "Health", "Join", "Give"
    };
    const char* items[9];
    std::string words[2];
    int topicOf[9];
    int count = 0;

    for (int i = 0; i < 7; ++i) {
        topicOf[count] = -1 - i;
        items[count++] = fixedTopics[i];
    }
    for (int k = 0; k < 2; ++k) {
        if (smsTopicUnlocked(st, func, ts, k, &words[k])) {
            topicOf[count] = k;
            items[count++] = words[k].c_str();
        }
    }

    int start = (st->lastPick < count) ? st->lastPick : 1;
    int n = EventHandler::chooseItem(items, count, start);
    if (n <= 0)
        return std::string();
    st->lastPick = n;

    screenMessage("%s\n", items[n]);
    int t = topicOf[n];
    if (t >= 0) {
        st->asked[t] = true;
        return std::string(func(ts, DS_TOPIC1 + t, NULL));
    }
    if (t == -4)
        st->askedJob = true;
    std::string in(fixedTopics[-1 - t]);
    in[0] = tolower(in[0]);
    return in;
}

void talkWordsClear() {
    talkHeardWords.clear();
}

bool talkWordsSave(FILE* fp) {
    std::set<std::string>::const_iterator it;
    for (it = talkHeardWords.begin(); it != talkHeardWords.end(); ++it)
        fprintf(fp, "%s\n", it->c_str());
    return ! ferror(fp);
}

void talkWordsLoad(FILE* fp) {
    char buf[64];
    talkHeardWords.clear();
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        while (len && isspace((unsigned char) buf[len-1]))
            buf[--len] = '\0';
        if (len)
            talkHeardWords.insert(buf);
    }
}

//---------------------------------------------------------------------------

static void talkYNResponse(TalkFunc func, TalkState* ts, SmsTalk* st)
{
    std::string input;
    int ans;

    while (1) {
        if (st) {
            static const char* yesNo[] = { "Yes", "No" };
            int n = EventHandler::chooseItem(yesNo, 2);
            if (n < 0)
                continue;
            screenMessage("%s\n", yesNo[n]);
            input = n ? "n" : "y";
        } else
            input = gameGetInput(3);
        screenCrLf();
        if (input.empty())
            return;
        ans = input[0];
        if (ans == 'y' || ans == 'Y') {
            ans = DS_ANSWER_Y;
            break;
        }
        if (ans == 'n' || ans == 'N') {
            ans = DS_ANSWER_N;
            break;
        }
        screenMessage("Yes or no!\n");
    }

    const char* reply = func(ts, ans, NULL);
    talkHeard(st, reply);
    screenMessage(reply);
    screenCrLf();
}

static void runTalkDialogue(TalkFunc func, TalkState* ts)
{
    std::string input;
    const char* in;
    const char* reply;
    SmsTalk sms;
    SmsTalk* st = NULL;

    if (smsInterface()) {
        st = &sms;
        smsInit(st, func, ts);
    }

#define DSTRING(V)  func(ts, V, NULL)
#define message     screenMessage
#define inputEq(K)  (strncasecmp(K, in, 4) == 0)

    message("\nYou meet %s\n", DSTRING(DS_LOOK));
    talkHeard(st, DSTRING(DS_LOOK));

    // 50% of the time they introduce themselves.
    if (xu4_random(2)) {
        screenCrLf();
        goto tell_name;
    }

    while (xu4.stage == StagePlay) {
        message("\nYour Interest:\n");
        input = st ? smsChooseTopic(st, func, ts) : gameGetInput(16);
        screenCrLf();
        screenCrLf();
        in = input.c_str();
        if (input.empty() || strncasecmp("bye", in, 3) == 0) {
            soundSpeakLine(ts->voiceStream, ts->startVoiceLn + VP_BYE);
            message("Bye.\n");
            break;
        }

        /* Does the person turn away from/attack you? */
        if (ts->turnAway) {
            int prob = xu4_random(0x100);
            if (prob < ts->turnAway) {
                if (ts->turnAway - prob < 0x40) {
                    message("%s turns away!\n", DSTRING(DS_PRONOUN));
                } else {
                    message("%s says: On guard! Fool!\n", DSTRING(DS_PRONOUN));
                    ts->person->movement = MOVEMENT_ATTACK_AVATAR;
                }
                break;
            }
        }

        reply = func(ts, DS_KEYWORD, input.c_str());
        if (reply) {
            talkHeard(st, reply);
            message(reply);
            screenCrLf();

            if (ts->nextOp == OP_PAUSE_ASK) {
                ts->nextOp = OP_NOP;

                EventHandler::waitAnyKey();

                reply = func(ts, DS_QUESTION, input.c_str());
                talkHeard(st, reply);
                message("\n%s\n\nYou say: ", reply);
                talkYNResponse(func, ts, st);
            }
        } else {
            // Standard responses.
            if (inputEq("look")) {
                talkHeard(st, DSTRING(DS_LOOK));
                message("You see %s\n", DSTRING(DS_LOOK));
            } else if (inputEq("name")) {
tell_name:
                soundSpeakLine(ts->voiceStream, ts->startVoiceLn + VP_I_AM);
                message("%s says: I am %s\n",
                        DSTRING(DS_PRONOUN), DSTRING(DS_NAME));
            } else if (inputEq("give")) {
                if (ts->person->getNpcType() == NPC_TALKER_BEGGAR) {
                    message("How much? ");
                    int gold = EventHandler::readInt(2);
                    screenCrLf();
                    if (gold > 0) {
                        if (c->party->donate(gold)) {
                            soundSpeakLine(ts->voiceStream,
                                           ts->startVoiceLn + VP_GIVE);
                            message("%s says: Oh Thank thee! I shall never "
                                    "forget thy kindness!\n",
                                    DSTRING(DS_PRONOUN));
                        } else
                            message("\n\nThou hast not that much gold!\n");
                    }
                } else {
                    soundSpeakLine(ts->voiceStream,
                                   ts->startVoiceLn + VP_GIVE);
                    message("%s says: I do not need thy gold.  Keep it!\n",
                            DSTRING(DS_PRONOUN));
                }
            } else if (inputEq("join")) {
                Virtue virt;
                const char* name = DSTRING(DS_NAME);
                if (c->party->canPersonJoin(name, &virt)) {
                    CannotJoinError join = c->party->join(name);
                    if (join == JOIN_SUCCEEDED) {
                        soundSpeakLine(ts->voiceStream,
                                       ts->startVoiceLn + VP_JOIN);
                        message("I am honored to join thee!\n");
                        c->location->map->removeObject(ts->person);
                        break;
                    }
                    message("Thou art not %s enough for me to join thee.\n",
                            (join == JOIN_NOT_VIRTUOUS) ?
                                getVirtueAdjective(virt) : "experienced");
                } else {
                    // Here we suppress the voice for the one companion of
                    // the player's class that cannot join.
                    if (ts->person->getNpcType() != NPC_TALKER_COMPANION) {
                        soundSpeakLine(ts->voiceStream,
                                       ts->startVoiceLn + VP_JOIN);
                    }
                    message("%s says: I cannot join thee.\n",
                            DSTRING(DS_PRONOUN));
                }
#if 1
            } else if (inputEq("ojna")) {
                /*
                 * This little easter egg appeared in the Amiga version of
                 * Ultima IV.  I've never figured out what the number means.
                 * "Banjo" Bob Hardy was the programmer for the Amiga version.
                 */
                message("Hi Banjo Bob!\nYour secret\nnumber is\n4F4A4E0A\n");
#endif
            } else
                message("That I cannot\nhelp thee with.\n");
        }
    }
}

//---------------------------------------------------------------------------

struct U4Talk {
    const char* strings;
    uint8_t  askAfter;
    uint8_t  questionHumility;
    uint8_t  turnAway;
    uint8_t  _pad;
    uint16_t name;
    uint16_t pronoun;
    uint16_t look;
    uint16_t job;
    uint16_t health;
    uint16_t response1;
    uint16_t response2;
    uint16_t question;
    uint16_t yes;
    uint16_t no;
    uint16_t topic1;
    uint16_t topic2;
};

enum QuestionTrigger {
    QT_NONE,
    QT_JOB = 3,
    QT_HEALTH = 4,
    QT_KEYWORD1 = 5,
    QT_KEYWORD2 = 6
};

/*
 * Return the number of bytes used for count strings.
 */
int U4Talk_setOffsets(uint16_t* offset, const char* strings, int initOff,
                      int count) {
    const char* ptr = strings + initOff;
    for (int i = 0; i < count; ++i) {
        *offset++ = ptr - strings;
        ptr += strlen(ptr) + 1;
    }
    return ptr - strings;
}

/*
 * Replace any trailing spaces with NUL.
 * If the keyword is "A   " then set the offset to zero to mark it as unused.
 */
static void trimKeyword(char* str, uint16_t* off) {
    char* cp = str + off[0];
    if (strcmp(cp, "A   ") == 0) {
        off[0] = 0;
    } else {
        while (*cp)
            ++cp;
        while (cp[-1] == ' ') {
            --cp;
            *cp = '\0';
        }
    }
}

/*
 * Return byte count of all strings.
 */
int U4Talk_load(U4FILE *file, U4Talk* tlk, char* tlkBuf)
{
    const int tlkSize = 288;
    const char* ptr;
    char* cp;
    int len;

    /* there's no dialogues left in the file */
    if (u4fread(tlkBuf, 1, tlkSize, file) != tlkSize)
        return 0;

    tlk->strings = tlkBuf;
    tlk->askAfter = tlkBuf[0];
    tlk->questionHumility = tlkBuf[1];
    tlk->turnAway = tlkBuf[2];

    len = U4Talk_setOffsets(&tlk->name, tlkBuf, 3, 12);

    // Fix the description string, converting the first character to lower-case.
    cp = tlkBuf + tlk->look;
    cp[0] = tolower(cp[0]);

    // ... then replace any newlines in the string with spaces
    while (*cp) {
        if (*cp == '\n')
            *cp = ' ';
        ++cp;
    }

    {
    const int EDIT_A = 1;
    const int EDIT_PERIOD = 2;

    // ... then append a period to the string if one does not already exist
    int edit = ispunct(cp[-1]) ? 0 : EDIT_PERIOD;

    // ... and finally, a few characters in the game have descriptions
    // that do not begin with a definite (the) or indefinite (a/an)
    // article.  On those characters, insert the appropriate article.
    ptr = tlkBuf + tlk->name;
    if (strcmp(ptr, "Iolo") == 0 ||
        strcmp(ptr, "Tracie") == 0 ||
        strcmp(ptr, "Dupre") == 0 ||
        strcmp(ptr, "Traveling Dan") == 0)
        edit |= EDIT_A;

    if (edit) {
        // Move look to the end of the loaded TLK data if there is room.
        int extLen = len + (tlk->job - tlk->look) + 3;
        if (extLen < tlkSize) {
            cp = tlkBuf + len;
            if (edit & EDIT_A) {
                *cp++ = 'a';
                *cp++ = ' ';
            }
            ptr = tlkBuf + tlk->look;
            while (*ptr)
                *cp++ = *ptr++;
            if (edit & EDIT_PERIOD)
                *cp++ = '.';
            *cp = '\0';

            tlk->look = len;
            len = cp - tlkBuf;
        }
    }
    }

    trimKeyword(tlkBuf, &tlk->topic1);
    trimKeyword(tlkBuf, &tlk->topic2);

    return len;
}

struct U4TalkState {
    TalkState state;
    const U4Talk* tlk;
};

static const char* U4Talk_dialogue(TalkState* ts, int value, const char* input)
{
    const U4Talk* tlk = ((U4TalkState*) ts)->tlk;

#define USTR(off)   (tlk->strings + tlk->off)
#define QUEUE_ASK(TRIGGER) \
    if (tlk->askAfter == TRIGGER) \
        ts->nextOp = OP_PAUSE_ASK;

    if (value == DS_KEYWORD) {
        const char* topic;

        if (tlk->topic1) {
            topic = USTR(topic1);
            if (strncasecmp(topic, input, strlen(topic)) == 0) {
                QUEUE_ASK(QT_KEYWORD1);
                return USTR(response1);
            }
        }

        if (tlk->topic2) {
            topic = USTR(topic2);
            if (strncasecmp(topic, input, strlen(topic)) == 0) {
                QUEUE_ASK(QT_KEYWORD2);
                return USTR(response2);
            }
        }

        if (strncasecmp("job", input, 3) == 0) {
            QUEUE_ASK(QT_JOB)
            return USTR(job);
        }
        if (strncasecmp("heal", input, 4) == 0) {
            QUEUE_ASK(QT_HEALTH)
            return USTR(health);
        }
    } else {
        switch(value) {
            case DS_NAME:
                return USTR(name);
            case DS_PRONOUN:
                return USTR(pronoun);
            case DS_LOOK:
                return USTR(look);
            case DS_QUESTION:
                return USTR(question);
            case DS_ANSWER_Y:
                if (tlk->questionHumility)
                    c->party->adjustKarma(KA_BRAGGED);
                return USTR(yes);
            case DS_ANSWER_N:
                if (tlk->questionHumility)
                    c->party->adjustKarma(KA_HUMBLE);
                return USTR(no);
            case DS_TOPIC1:
                return tlk->topic1 ? USTR(topic1) : NULL;
            case DS_TOPIC2:
                return tlk->topic2 ? USTR(topic2) : NULL;
            case DS_TEXT_JOB:
                return USTR(job);
            case DS_TEXT_HEALTH:
                return USTR(health);
            case DS_TEXT_R1:
                return USTR(response1);
            case DS_TEXT_R2:
                return USTR(response2);
            case DS_TEXT_QUESTION:
                return USTR(question);
        }
    }
    return NULL;
}

void talkRunU4Tlk(const Discourse* disc, int conv, Person* person)
{
    U4TalkState ts;

    ts.tlk = ((const U4Talk*) disc->conv.table) + conv;
    ts.state.person = person;
    ts.state.turnAway = ts.tlk->turnAway;
    ts.state.nextOp = OP_NOP;
    ts.state.voiceStream = 0;
    ts.state.startVoiceLn = 0;

    runTalkDialogue(U4Talk_dialogue, &ts.state);
}

//---------------------------------------------------------------------------

#ifdef USE_BORON
#include <boron/urlan.h>

enum BoronDialogueIndex {
    DI_NAME,
    DI_PRONOUN,
    DI_LOOK,
    DI_DATA,        // turn-away, voice-stream, voice-line
    DI_TOPICS,
    DI_COUNT
};

struct BoronDialogue {
    TalkState state;
    UThread* ut;
    const UCell* values;
    const UCell* askCell;
    UBlockIt topics;
};

static inline bool wordQuestionHumility(UThread* ut, const UCell* cell)
{
    return strcmp(ur_wordCStr(cell), "ask-humility") == 0;
}

static const char* dialogueBoron(TalkState* ts, int value, const char* input)
{
    BoronDialogue* bd = (BoronDialogue*) ts;
    UThread* ut = bd->ut;
    USeriesIter si;
    UIndex n;

    if (value == DS_KEYWORD) {
        int voiceLine = ts->startVoiceLn + VP_KEYWORD;
        const UCell* cell = bd->topics.it;
        const UCell* end  = bd->topics.end;

        while (cell != end) {
            if (ur_is(cell, UT_STRING)) {
                ur_seriesSlice(ut, &si, cell);
                n = si.end - si.it;
                if (strncasecmp(si.buf->ptr.c + si.it, input, n) == 0) {
                    ur_seriesSlice(ut, &si, ++cell);

                    // Queue up any following question.
                    ++cell;
                    if (cell != end && ur_is(cell, UT_WORD)) {
                        bd->askCell = cell;
                        ts->askVoiceLn = voiceLine + 1;
                        ts->nextOp = OP_PAUSE_ASK;
                    }
                    soundSpeakLine(ts->voiceStream, voiceLine);
                    goto reply;
                }
                cell += 2;
                voiceLine += 1;
            } else if (ur_is(cell, UT_WORD)) {
                // Skip (ask "Question?" ["Yes reply" "No reply"])
                cell += 3;
                voiceLine += 3;
            }
        }
        return NULL;
    }

    if (value >= DS_TOPIC1)
        return NULL;    // SMS topic menu strings are not supported yet.

    switch (value) {
        case DS_QUESTION:
            if (! bd->askCell)
                return NULL;
            soundSpeakLine(ts->voiceStream, ts->askVoiceLn);
            ur_seriesSlice(ut, &si, bd->askCell + 1);
            break;
        case DS_ANSWER_Y:
            n = 0;
            goto answer;
        case DS_ANSWER_N:
            n = 1;
answer:
            if (! bd->askCell)
                return NULL;
            if (wordQuestionHumility(ut, bd->askCell))
                c->party->adjustKarma(n ? KA_HUMBLE : KA_BRAGGED);
            soundSpeakLine(ts->voiceStream, ts->askVoiceLn + n + 1);
            {
            const UBuffer* blk = ur_bufferSer(bd->askCell + 2);
            ur_seriesSlice(ut, &si, blk->ptr.cell + n);
            }
            bd->askCell = NULL;
            break;
        default:
            ur_seriesSlice(ut, &si, bd->values + value);
            break;
    }

reply:
    //assert(si.buf->form != UR_ENC_UCS2);
    return si.buf->ptr.c + si.it;
}

void talkRunBoron(int32_t discBlkN, int conv, Person* person)
{
    BoronDialogue bd;
    UThread* ut = xu4.config->boronThread();
    const UBuffer* blk = ur_buffer(discBlkN);

    bd.ut = ut;
    bd.values = blk->ptr.cell + conv * DI_COUNT;
    bd.askCell = NULL;
    ur_blockIt(ut, &bd.topics, bd.values + DI_TOPICS);

    {
    const UCell* data = bd.values + DI_DATA;
    TalkState* ts = &bd.state;

    ts->person = person;
    ts->turnAway = data->coord.n[0];
    ts->nextOp = OP_NOP;
    ts->voiceStream = data->coord.n[1];
    if (ts->voiceStream)
        ts->voiceStream++;       // Config::musicFile() id is one based.
    ts->startVoiceLn = data->coord.n[2];
    }

    runTalkDialogue(dialogueBoron, &bd.state);
}

const char* talkNameBoron(int32_t discBlkN, int conv)
{
    UThread* ut = xu4.config->boronThread();
    const UBuffer* blk = ur_buffer(discBlkN);
    const UCell* values = blk->ptr.cell + conv * DI_COUNT;

    USeriesIter si;
    ur_seriesSlice(ut, &si, values + DI_NAME);
    return si.buf->ptr.c + si.it;
}
#endif

# Skip the title, start a new game as "Tester" (male), page through the
# story & answer every gypsy question with A, then cancel the Attack prompt
# left by the extra key presses.
seed 1234
wait 60
key esc
wait 30
key esc
wait 30
key i
wait 30
type Tester
key enter
wait 20
key m
wait 20
gap 40
repeat 18 space
gap 60
repeat 26 a
wait 20
key esc
gap 4

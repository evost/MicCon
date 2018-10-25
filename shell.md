# MicCon shell (mcsh)

## System
- asm F			- *compile file into bytecode (first character of new file will be "~" or "_")*
- b+				- *increase backlight*
- b-				- *decrease backlight*
- cat F			- *show file*
- cd D				- *change working directory*
- clear			- *clear the display*
- color FB			- *set font and background colors*
- date DDMMYYHHMM	- *set date and time as DDMMYYHHMM*
- delay expression	- *wait (in milliseconds)*
- echo expression	- *print out the expression*
- hex F			- *show file into hex mode*
- input A			- *let the user input an variable*
- let A expression	- *assign value to a variable*
- ls D				- *file listing*
- mk F				- *make file*
- mkdir D			- *make directory*
- nano F			- *text editor*
- pwd				- *print current directory*
- rm F				- *remove file*
- rmdir D			- *remove directory*
- run F			- *execution of file with bytecode*
- sh F				- *start script*
- uptime			- *display uptime in seconds*
- ' stuff			- *remark/comment*

## Expressions, Math
- +, -, \*, /, %, pow		- *Math*
- \`					- *unary minus sign*
- ~					- *unary bitwise NOT*
- <<, >>, |, ^, &		- *Bitwise*
- <, <=, =, !, >=, >	- *Comparisons*
- a..z					- *value of var*
- time					- *times as seconds since 1/1/2000*
- free					- *value of free bytes of RAM*
- rand					- *random*

## Control (only in script)
- end					- *exit from script*
- if/expression/strnum	- *if expression != 0 goto strnum*
- goto/strnum			- *continue execution at strnum*
(/ - new line)

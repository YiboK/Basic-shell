# Basic-shell

A basic shell with commands: cd, pwd, exit, redirection (">"), pipe ("|"), loop, multiple commands (";")

Usage of Program:
smash

### Usage of loop: 

"loop 3 pwd" will execute pwd 3 times.

### For other commands, use "/bin/'cmd'".

Example: "/bin/ls" will execute ls in shell

All commands can be combined: 
"ls test.txt | /bin/grep test > output.txt"
"loop 5 cmd1 > output"
"ls test.txt | /bin/grep test ; ls test.csv | /bin/grep column"

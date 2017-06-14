# ftserver and ftclient are in separate folders

# compile ftserver
`gcc -o ftserver ftserver.c`

# start ftserver
`./ftserver 12345`

# start ftclient and list directory
`java ftclient localhost 12345 -l 23456`

# get file
`java ftclient localhost 12345 -g <filename> 23456`

# Usage
ftserver must be started first. 
ftclient can connect to ftserver for directory list or retrieve file.
The connections close after request complete. 



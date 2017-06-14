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
```

SERVER (flip1)                                  CLIENT (flip2)
Input to console           Output               Input to Console         Output

> ftserver 30021
                     Server open on 30021
                                               > ftclient flip1 30021 –l 
                     Connection from flip2.
                     List directory requested
                     on port 30020.
                     Sending directory
                     contents to flip2:30020

                                                                         Receiving directory
                                                                         structure from
                                                                         flip1:30020

                                                                         shortfile.txt
                                                                         longfile.txt

                                               > ftclient flip1 30021 –g
                                               shortfile.txt 30020

 Connection from flip2.
File “shortfile.txt”
requested on port 30020.
Sending “shortfile.txt”
to flip2:30020

                                                                         Receiving
                                                                         “shortfile.txt”
                                                                         from flip1:30020

                                                                         File transfer
                                                                         complete.

                                                 > ftclient flip1 30021 –
                                                 longfileee.txt 30020

 Connection from flip2.

File “longfileee.txt”
requested on port 30020.
File not found. Sending
error message to
flip2:30021

                                                                         flip1:30021 says
                                                                         FILE NOT FOUND

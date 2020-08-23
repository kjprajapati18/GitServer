# GitServer
Where's the File? Server/Client Challenge

Coding a smaller scale git with C. Practice with sockets, threading, and forking.

Note that this program only works on Linux machines. To run, download all repository files to a directory on your machine. Then, run the command "make all" (makefile will compile necessary files). After successful compilation, simply use:
  
  ./WTFServer (port) to start the server
  
  ./WTF configure (IP) (port>) to configure client connection

If the server is running and the client is correctly configured, then the client can start using the server like they use Git or Github. See Readme.pdf for more information on program structure, as well as descriptions on every available command.

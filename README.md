# HTTP File Sharing
Easily share a file by sending a simple URL to the recipient.

## Usage
To send the file "Test" from a machine with IP address 1.2.3.4 to another machine, do the following :  
On the sender machine, type :
```
http-file-sharing Path/To/File/Test
```
and send the URL "1.2.3.4:8080" to the recipient machine.  

On the recipient machine, simply paste the URL "1.2.3.4:8080" into a web browser to start downloading the file.  

## Contribution
This program is heavily inspired by the well-known [WOOF](http://www.home.unix-ag.org/simon/woof.html) command written by Simon Budig.

# FTP Server using python

You can build FTP Server using python.   
Tutorial is [here](https://pyftpdlib.readthedocs.io/en/latest/tutorial.html).   

# Installation   
```
cd esp-idf-ftpClient/python-ftp-server
python3 -m pip install pyftpdlib

python3 main.py --help
usage: main.py [-h] [--user USER] [--password PASSWORD] [--port PORT]

optional arguments:
  -h, --help           show this help message and exit
  --user USER          ftp user name
  --password PASSWORD  ftp user password
  --port PORT          ftp port
```

# Default parameters   
- user:ftpuser   
- password:ftppass   
- port:2121   

# Screen Shot
```
[I 2024-04-11 21:54:51] concurrency model: async
[I 2024-04-11 21:54:51] masquerade (NAT) address: None
[I 2024-04-11 21:54:51] passive ports: None
[I 2024-04-11 21:54:51] >>> starting FTP server on 0.0.0.0:2121, pid=4057498 <<<
[I 2024-04-11 21:55:00] 192.168.10.123:58842-[] FTP session opened (connect)
[I 2024-04-11 21:55:00] 192.168.10.123:58842-[ftpuser] USER 'ftpuser' logged in.
[I 2024-04-11 21:55:00] 192.168.10.123:58842-[ftpuser] STOR /home/nop/rtos/esp-idf-ftpClient/python-ftp-server/hello.txt completed=1 bytes=14 seconds=0.019
on_file_received=/home/nop/rtos/esp-idf-ftpClient/python-ftp-server/hello.txt
[I 2024-04-11 21:55:00] 192.168.10.123:58842-[ftpuser] RETR /home/nop/rtos/esp-idf-ftpClient/python-ftp-server/hello.txt completed=1 bytes=14 seconds=0.02
on_file_sent=/home/nop/rtos/esp-idf-ftpClient/python-ftp-server/hello.txt
[I 2024-04-11 21:55:00] 192.168.10.123:58842-[ftpuser] FTP session closed (disconnect).
```

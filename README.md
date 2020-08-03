# RDMA Cache-Eviction Attack Measurment Utility 

## tl;dr

### Network structure
```
+-----------------+              +-----------------+
|                 |              |                 |
|     Server      +<------------>+     Client      |
|   192.168.0.1   |              |   192.168.0.2   |
+--------+--------+              +-----------------+
         ^
         |
         |
         v
+--------+--------+
|                 |
|    Attacker     |
|  192.168.0.3    |
+-----------------+

```
### Build
```shell
$ git clone <repository_url>
$ cd <repository_directory>
$ mkdir build
$ cmake ../
$ make
```
### Commands to execute
1. On server
   ```bash
   $ sudo ./main -l -p 1234
   ```
2. On client
    ```bash
    $ sudo ./main -l -p 1234 -a 192.168.0.1
    ```
3. Watch current latency for a single-byte RDMA read. (In microseconds) The latency is displayed at the client window.
4. Now we will start the attack and watch the latency increase.
5. On server
    ```bash
    $ sudo ./main -e -p 4321
    ```
6. On attacker
   ```bash
   $ sudo ./main -e -p 4321 -a 192.168.0.1
   ```

### Use help
```
Usage: ./main [-a server_addr] [-p port] [-l | -e] [-h]
	 -h - print this help and exit
	 -a - set to client mode and specify the server's IP address, otherwise - server mode.
	 -p - specify the port number to connect to (default: 12345)
	 -l - latency measurement mode
	 -e - cache exhauster mode
```
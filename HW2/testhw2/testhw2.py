import socket
import sys
import os
import subprocess
import time
from datetime import date

# save client socket
sock_dict = {}

# replace {date} in template_filename to correct date and write to output_filename 
def replace_date(template_filename, output_filename):
    with open(template_filename,"rt") as f1:
        data = f1.read()
        data = data.replace("{date}",date.today().strftime("%m/%d"))
        with open(output_filename,"wt") as f2:
            f2.write(data)

# create new socket and connect to server
def get_sock(port):
    addr = ("127.0.0.1", port)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(addr)
    return sock

# start server in path on port
def set_server(path, port):
    proc_args = [path, str(port)]
    proc = subprocess.Popen(proc_args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(0.1)
    return proc

def testing(server_path, filename, p):
    try:
        # use subprocess to start server on port p
        server = set_server(server_path, p)
        print(f"open server {server_path} on localhost {p}")
        msg = ''
        # read input file
        with open(f'input/{filename}', 'r') as f:
            print(f"open file input/{filename}")
            while input := f.readline():
                if input[0].isnumeric() == False:
                    continue
                # parse client number
                space_index = input.find(' ')
                client_num = input[:space_index] 
                # when the client is new, create a new socket and connect to server
                if sock_dict.get(client_num) == None:
                    #print(f"client {client_num} connect to server")
                    sock_dict[client_num] = get_sock(p)                
                #print(f"send message to client {client_num}: {input[space_index+1:]}")
                # send command to server
                s = input[space_index+1:].encode()
                sock_dict[client_num].send(s)
                time.sleep(0.1)
                sock_dict[client_num].settimeout(3)
                try:
                    # receive message from server and append message to msg if message length > 0
                    m = sock_dict[client_num].recv(1024).decode().replace('\x00','')
                    if len(m) > 0: 
                        #print(f"receive message: {m}")
                        msg += m     
                except Exception as ex:
                    print(ex)
                    pass
                # close client socket when the input command is exit
                if input[space_index+1:].replace("\n","") == "exit":
                    sock_dict[client_num].close()
                    del sock_dict[client_num]
        # close the remaining sockets after seading all the input
        for key in sock_dict:
            sock_dict[key].close()
        sock_dict.clear()
        # write msg including all the received message to output
        postfix = filename.replace("input_","")
        print(f"write output result to output/output_{postfix}")
        with open(f'output/output_{postfix}', 'w') as f:
            f.write(msg)
        time.sleep(0.1)
        # close the server
        server.kill()
        # use diff to compare output with answer
        print(f"write diff result to diff/diff_{postfix}")
        os.system(f'diff -w -E -B output/output_{postfix} answer/answer_{postfix} > diff/diff_{postfix}')
        # return result according to diff file size
        size = os.path.getsize(f"diff/diff_{postfix}")
        #print(f"file size: {size}")
        if size > 0:
            return False
        else:
            return True
    except Exception as ex:
        print(ex)
        time.sleep(0.1)
        # kill server if there is any exception
        server.kill()
    return False

def main():
    # check parameter
    if len(sys.argv) == 3:
        try: 
            # count records port increase number
            count = 0
            # replace {date} in answer_single_template to correct date and output to answer_single
            replace_date("answer/answer_single_template","answer/answer_single")
            # set input directory
            directory = "input/"
            # traverse input directory for file name starting with input_
            for entry in os.scandir(directory):
                if entry.name.startswith("input_") and entry.is_file():
                    # run test case and output result
                    if testing(sys.argv[1],entry.name,int(sys.argv[2])+count):
                        print(f"testcase {entry.name}: correct")
                    else:
                        print(f"testcase {entry.name}: wrong")
                    # increase port number
                    count += 1
        except Exception as ex:
            print(ex)
    else:
        print("Usage: python3 testhw2.py <server_path> <port>")

if __name__ == '__main__':
    main()
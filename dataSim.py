import serial
import random
import time
import threading
#create a mutex struct for locking the critical section
mu=threading.Lock()
#function that builds, simulates, and transmits
#fake data packets (ERPA, PMT, HK)
#NOTE please pip install serial
def simPacket(header, length, interval, comPort):
    while True:
        #build a randomized packet data set to attach to header
        packet = [random.randint(0,255) for _ in range(length-2)]
        #preempt data with appropriate header
        packet.insert(0, (header>>8)&0xFF)
        packet.insert(1, header&0xFF)
        #conversion to bytes
        packet_byte_conv = bytes(packet)
            
        #send via UART
        with mu:
            try:
                with serial.Serial(comPort, baudrate=57600, timeout=1) as s:
                    s.write(packet_byte_conv)
                    print(f"transmit success: {packet_byte_conv}")
                    #sleep for the proper interval for specified data packet

                    time.sleep(interval/1000) 
            except Exception as e:
                print(f"ERROR OCCURRED: {e}") 
                break      

#first ask the user for the COM Port in use
comPort = input("Enter COM Port in use (ex. COMx): ") 
#intialization of the threads
ERPA_thread = threading.Thread(target=simPacket, args=(0xAAAA, 14, 100, comPort))
PMT_thread = threading.Thread(target=simPacket, args=(0xBBBB, 6, 125, comPort))
HK_thread = threading.Thread(target=simPacket, args=(0xCCCC, 22, 100, comPort))
#start all of the threads
ERPA_thread.start()
PMT_thread.start()
HK_thread.start()



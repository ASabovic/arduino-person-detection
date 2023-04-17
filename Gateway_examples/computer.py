"""
This script enables the IoT gateway to receieve the capture image or final local inference results from the Arduino board (or any other connected device).
"""
import asyncio
from datetime import datetime
import os 
import io
from typing import Any, Callable
from PIL import Image
from bleak import BleakClient

#BLE configuration
#Must be the same as service uuid used with the Arduino board
service_uuid = "SERVICE_UUID" #example -> service_uuid = "0000180f-0000-1000-8000-00805f9b34fb"
#Must be the same as TX CHAR UUID used with the Arduino board
message_uuid = "MESSAGE_UUID" #example -> message_uuid = "2d2F88c4-f244-5a80-21f1-ee0224e80658"
#MAC address of the Arduino board
address = "MAC_ADDRESS" #example -> address = "8D:3E:BD:BE:13:3E"
data_buffer_size = 4096
output_file = "captured_data.txt"

class ArduinoConnection:

    client: BleakClient = None

    def __init__(
        self,
        loop: asyncio.AbstractEventLoop,
        message_uuid: str,
        service_uuid: str,
        data_buffer_cleaner: Callable[[str, Any], None],
        data_buffer_size: int
    ):

        self.loop = loop
        self.message_uuid = message_uuid
        self.service_uuid = service_uuid
        self.data_buffer_cleaner = data_buffer_cleaner
        self.data_buffer_size = data_buffer_size

        self.connected = False
        self.connected_device = None 
        self.connection_enabled = True
        self.onConnection = True
        self.char_object = str

        self.last_packet_time = datetime.now()
        self.rx_data = []
        self.rx_timestamp = []
        self.rx_delays = []
        self.dataMap = bytearray()
        self.dataByte = bytearray()
        self.dataLocal = Any
        self.data_to_send = Any
        self.counter = 0
        self.bytecounter = 0
        self.m = 0
        self.n = 219

    #This function is called every time the BleakClient loses the connection with the remote device (Arduino board in our case).
    def on_disconnect(self, client: BleakClient):
        self.connected = False
        self.onConnection = False

        # print("I am in the on_disconnect function!")
    
    def notification_handler(self, sender: str, data: Any):
        
        # print(len(data))

        if (len(data) == 220):
            self.dataByte = data
            self.dataMap[self.m:self.n] = self.dataByte
            self.counter=self.counter+1
            self.bytecounter=self.bytecounter+1
            self.m = self.m+220
            self.n = self.n+220
            # print(len(self.dataMap))

            if(self.counter==14 and len(self.dataMap) == 3080):
                # print("Ready to process the image of 3080 bytes")
                picture_stream = io.BytesIO(self.dataMap)
                data_to_send = Image.open(picture_stream)
                data_to_send = data_to_send.resize((320, 240))
                data_to_send.save("result.png")

                self.counter=0
                self.bytecounter=0
                self.m = 0
                self.n = 219
                self.dataMap.clear()

        elif (len(data) == 1 and data.decode("utf-8") == "7"):
            # print("I noticed 2 bytes!")
            # print("Ready to process the image of 2056 bytes")
            self.dataMap = self.dataMap[0:2056]

            self.counter=0
            self.bytecounter=0
            self.m = 0
            self.n = 219
            self.dataMap.clear()
        
        elif(len(data) == 1):
            self.dataLocal = data.decode("utf-8")
            if self.dataLocal == "1":
                self.data_to_send = "Person is detected -> Local inference!"
                print(self.data_to_send)

            else: 
                self.data_to_send = "Person is not detected -> Local inference!"
                print(self.data_to_send)
        
        else:
            self.dataLocal = data.decode("utf-8")
            print(self.dataLocal)

    async def manager(self):
        print("Starting connection manager.")
        while True:
            if self.client and self.onConnection==True:
                #print("I am here and ready to be connected!")
                #await self.select_device()
                await self.connect()
            else:
                #print("I am here and I am looking for new devices!")
                await self.select_device()
                await asyncio.sleep(1.0, loop=loop)
                self.onConnection = True
    
    async def connect(self):
        #while True:
        if self.onConnection==True:
            if self.connection_enabled:
                try:
                    await self.client.connect()
                    for service in self.client.services:
                        if service == self.service_uuid:
                            service = self.service_uuid
                        for char in service.characteristics:
                            if char == self.message_uuid:
                                char = self.message_uuid
                        
                    self.char_object = char
                    self.connected = await self.client.is_connected()
                    if self.connected:
                        print("Connected to Device")
                        self.client.set_disconnected_callback(self.on_disconnect)
                        await self.client.start_notify(
                            self.char_object, self.notification_handler,
                        )
                        while True:
                            if not self.connected:
                                break
                            print("From on_disconnect function I arrived here!")
                            await asyncio.sleep(1.0)
                    else:
                        print(f"Failed to connect to Device")
                except Exception as e:
                    print(e)
            else:
                await asyncio.sleep(1.0)
        else:
            await self.manager()
    
    async def cleanup(self):
        if self.client:

            await self.client.disconnected()
    
    async def select_device(self):
        print("Bluetooh LE hardware warming up...")
        await asyncio.sleep(2.0, loop=loop)
        self.client = BleakClient(address)

class DataToFile:

    column_name = ["data_value"]

    def __init__(self, write_path):
        self.path = write_path
    
    def write_to_txt(self, data_values: Any):
        file = open(output_file, "a")
        file.write(data_values + '\n')
        file.close()

async def main():
    while True:
        await asyncio.sleep(1000)

        
if __name__ == "__main__":

    os.environ["PYTHONASYNCIODEBUG"] = str(1)
    loop = asyncio.get_event_loop()
    data_to_file = DataToFile(output_file)
    connection = ArduinoConnection(loop, message_uuid, service_uuid, data_buffer_size, data_to_file.write_to_txt)

    try:
        asyncio.ensure_future(main())
        asyncio.ensure_future(connection.manager())
        loop.run_forever()
    except KeyboardInterrupt:
        print("User stopped the connection!")
    finally:
        print("Disconnecting....")
        loop.run_until_complete(connection.cleanup())
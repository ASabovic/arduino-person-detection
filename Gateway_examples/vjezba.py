"""
This script enables the IoT gateway to scan for available devices and connect with the right Arduino board. Once the connection is established, the IoT gateway starts receiving 
packets of bytes that are either local inference result or captured image. In case, the received data is the local inference result, it will be displayed and confirmed. Otherwise,
if received data is captured image, it will be edited and processed as a input to our heavy-weight ML model for remote inferencing. Once the result from remote inference is 
available, it will be sent back to the Arduino board via BLE.
"""
import asyncio
from datetime import datetime
import os 
import io
from typing import Any, Callable
from PIL import Image
from bleak import BleakClient
import h5py
import cv2
import tensorflow as tf 
import numpy as np
from tensorflow.keras.models import load_model
import time

#BLE configuration
#Must be the same as service uuid used with the Arduino board
service_uuid = "SERVICE_UUID" #example -> service_uuid = "0000180f-0000-1000-8000-00805f9b34fb"
#Must be the same as TX CHAR UUID used with the Arduino board
message_uuid = "MESSAGE_UUID" #example -> message_uuid = "2d2F88c4-f244-5a80-21f1-ee0224e80658"
#Must be the same as RX CHAR UUID used with the Arduino board
write_characteristic = "WRITE_CHAR" #example -> write_characteristic = "00002A3D-0000-1000-8000-00805f9b34fb"
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
        write_characteristic: str,
        data_buffer_cleaner: Callable[[str, Any], None],
        data_buffer_size: int
    ):

        self.loop = loop
        self.message_uuid = message_uuid
        self.service_uuid = service_uuid
        self.write_characteristic = write_characteristic
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
        self.results_counter = 0

        filename = 'facetracker.h5'
        with h5py.File(filename, 'r') as f:
            a_group_key = list(f.keys())[0]
            data = list(f[a_group_key])
        
        self.facetracker = load_model('facetracker.h5')

    #This function is called every time the BleakClient loses the connection with the remote device!
    def on_disconnect(self, client: BleakClient):
        self.connected = False
        self.onConnection = False
    
    async def notification_handler(self, sender: str, data: Any):
    
        if (len(data) == 220):
            self.dataByte = data
            self.dataMap[self.m:self.n] = self.dataByte
            self.counter=self.counter+1
            self.bytecounter=self.bytecounter+1
            self.m = self.m+220
            self.n = self.n+220

            if(self.counter==14 and len(self.dataMap) == 3080):
                
                st = time.process_time()
                picture_stream = io.BytesIO(self.dataMap)
                data_to_send = Image.open(picture_stream)
                data_to_send = data_to_send.resize((320, 240))
                timestr = time.strftime("%Y-%m-%d_%H-%M-%S")

                img_name = "{date}-result.jpg".format(date=timestr)
                data_to_send.save(img_name)

                cap = cv2.imread(data_to_send)
                rgb = cv2.cvtColor(cap, cv2.COLOR_BGR2RGB)
                resized = tf.image.resize(rgb, (120,120))
                yhat = self.facetracker.predict(np.expand_dims(resized/255,0))
                sample_coords = yhat[1][0] 
                if yhat[0] > 0.5: 
                    cv2.rectangle(cap, 
                                tuple(np.multiply(sample_coords[:2], [cap.shape[1],cap.shape[0]]).astype(int)),
                                tuple(np.multiply(sample_coords[2:], [cap.shape[1],cap.shape[0]]).astype(int)), 
                                        (255,0,0), 2)
                    cv2.rectangle(cap, 
                                tuple(np.add(np.multiply(sample_coords[:2], [cap.shape[1],cap.shape[0]]).astype(int), 
                                        [0,-30])),
                                tuple(np.add(np.multiply(sample_coords[:2], [cap.shape[1],cap.shape[0]]).astype(int),
                                        [80,0])), 
                                    (255,0,0), -1)
                    cv2.putText(cap, 'face', tuple(np.add(np.multiply(sample_coords[:2], [cap.shape[1],cap.shape[0]]).astype(int),
                                        [0,-5])),
                                    cv2.FONT_HERSHEY_SIMPLEX, 1, (255,255,255), 2, cv2.LINE_AA)
                
                remote_inference = int(yhat[0]*1000)
                res = str(remote_inference)
                result = res.encode()
                await self.client.write_gatt_char(self.write_characteristic, result)
                self.results_counter = self.results_counter+1
                currtime = time.strftime("%Y-%m-%d_%H-%M-%S")
                current_time = "{date}".format(date=currtime)
                # print("Remote inference time is: ")
                # print(current_time)

        elif(len(data) == 1 and data.decode("utf-8") == "7"):
            
            if(len(self.dataMap)) < 2050:
               pass

            else:
                self.dataMap = self.dataMap[0:2056]

                st = time.process_time()
                picture_stream = io.BytesIO(self.dataMap)
                data_to_send = Image.open(picture_stream)
                data_to_send = data_to_send.resize((320, 240))
                timestring = time.strftime("%Y-%m-%d_%H-%M-%S")

                image_name = "{date}-result.jpg".format(date=timestring)
                data_to_send.save(image_name)

                cap = cv2.imread(data_to_send)
                rgb = cv2.cvtColor(cap, cv2.COLOR_BGR2RGB)
                resized = tf.image.resize(rgb, (120,120))
                yhat = self.facetracker.predict(np.expand_dims(resized/255,0))
                sample_coords = yhat[1][0] 
                if yhat[0] > 0.5: 
                    cv2.rectangle(cap, 
                            tuple(np.multiply(sample_coords[:2], [cap.shape[1],cap.shape[0]]).astype(int)),
                            tuple(np.multiply(sample_coords[2:], [cap.shape[1],cap.shape[0]]).astype(int)), 
                                    (255,0,0), 2)
                    cv2.rectangle(cap, 
                            tuple(np.add(np.multiply(sample_coords[:2], [cap.shape[1],cap.shape[0]]).astype(int), 
                                    [0,-30])),
                            tuple(np.add(np.multiply(sample_coords[:2], [cap.shape[1],cap.shape[0]]).astype(int),
                                    [80,0])), 
                                    (255,0,0), -1)
                    cv2.putText(cap, 'face', tuple(np.add(np.multiply(sample_coords[:2], [cap.shape[1],cap.shape[0]]).astype(int),
                                    [0,-5])),
                                cv2.FONT_HERSHEY_SIMPLEX, 1, (255,255,255), 2, cv2.LINE_AA)
                
                remote_inference = int(yhat[0]*1000)
                res = str(remote_inference)
                result = res.encode()
                await self.client.write_gatt_char(self.write_characteristic, result)
                self.results_counter = self.results_counter+1
                currtime2 = time.strftime("%Y-%m-%d_%H-%M-%S")
                current_time = "{date}".format(date=currtime2)
                # print("Remote inference time is: ")
                # print(current_time)
        
        elif(len(data) == 1):
            self.dataLocal = data.decode("utf-8")
            if self.dataLocal == "1":
                self.data_to_send = "Person is detected -> Local inference!"
                self.results_counter = self.results_counter+1
            else: 
                self.data_to_send = "Person is not detected -> Local inference!"
                self.results_counter = self.results_counter+1        
        else:
            print("No data available at the moment!")

    async def manager(self):
        while True:
            if self.client and self.onConnection==True:
                await self.connect()
            else:
                await self.select_device()
                await asyncio.sleep(1.0, loop=loop)
                self.onConnection = True
    
    async def connect(self):
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
                        self.client.set_disconnected_callback(self.on_disconnect)
                        await self.client.start_notify(
                            self.char_object, self.notification_handler,
                        )
                        self.counter=0
                        self.bytecounter=0
                        self.m = 0
                        self.n = 219
                        self.dataMap.clear()
                        while True:
                            if not self.connected:
                                break
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

    connection = ArduinoConnection(loop, message_uuid, service_uuid, write_characteristic, data_buffer_size, data_to_file.write_to_txt)

    try:
        asyncio.ensure_future(main())
        asyncio.ensure_future(connection.manager())
        loop.run_forever()
    except KeyboardInterrupt:
        print("User stopped the connection!")
    finally:
        print("Disconnecting....")
        loop.run_until_complete(connection.cleanup())
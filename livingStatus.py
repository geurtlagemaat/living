# -*- coding: utf-8 -*-
import traceback
from twisted.internet import reactor
import Adafruit_DHT
import serial

def doUpdate(NodeControl):
    NodeControl.log.debug("Weather Status update")
    try:
        # DS18B20
        if NodeControl.nodeProps.has_option('sensordata', 'GrondSensorPath'):
            mySensorReading = getDS18B20Temp(NodeControl, NodeControl.nodeProps.get('sensordata', 'GrondSensorPath'))
            if mySensorReading is not None:
                sGrondTemp = '{:.1f}'.format(mySensorReading  / float(1000))
                NodeControl.log.debug("Grond temperatuur: %s" % sGrondTemp)
                NodeControl.MQTTPublish(sTopic="living/vloer", sValue=str(sGrondTemp), iQOS=0, bRetain=True)
            else:
                NodeControl.log.warning("no reading: %s" % NodeControl.nodeProps.get('sensordata', 'GrondSensorPath'))
        else:
            NodeControl.log.warning("Can not read grondtemp., no [sensordata] GrondSensorPath configured")

        # DHT22
        if NodeControl.nodeProps.has_option('sensordata', 'DHT22Pin'):
            try:
                getDHT22Data(NodeControl, NodeControl.nodeProps.get('sensordata', 'DHT22Pin'), 0)
            except Exception, exp:
                NodeControl.log.warning("Error temp/hum status update, error: %s." % (traceback.format_exc()))
        else:
            NodeControl.log.warning("Can not read DHT sensor, no [sensordata] DHT22Pin configured")
        # CO2
        if NodeControl.nodeProps.has_option('sensordata', 'co2port'):
            try:
                getCO2(NodeControl, NodeControl.nodeProps.get('sensordata', 'co2port'))
            except Exception, exp:
                NodeControl.log.warning("Error reading sensor, path: %s, error: %s." %\
                                        (NodeControl.nodeProps.get('sensordata', 'co2port'), traceback.format_exc()))
        else:
            NodeControl.log.info("CO2 sensor path ([sensordata] co2port) not found in settings")
    except Exception, exp:
        NodeControl.log.warning("Error status update, error: %s." % (traceback.format_exc()))

def getDHT22Data(NodeControl, Pin, iTry):
    sensor = Adafruit_DHT.DHT22
    humidity, temperature = Adafruit_DHT.read_retry(sensor, Pin)
    if humidity is not None and temperature is not None:
        Temp = '{:.1f}'.format(temperature)
        Hum = '{:.1f}'.format(humidity)
        NodeControl.MQTTPublish(sTopic="living/temp", sValue=str(Temp), iQOS=0, bRetain=True)
        NodeControl.MQTTPublish(sTopic="living/hum", sValue=str(Hum), iQOS=0, bRetain=True)
    else:
        if iTry < 15:
            reactor.callLater(2, getDHT22Data, NodeControl, Pin, iTry+1)

def getDS18B20Temp(NodeControl, sSensorPath):
    mytemp = None
    try:
        f = open(sSensorPath, 'r')
        line = f.readline() # read 1st line
        crc = line.rsplit(' ', 1)
        crc = crc[1].replace('\n', '')
        if crc == 'YES':
            line = f.readline() # read 2nd line
            mytemp = line.rsplit('t=', 1)
        else:
            NodeControl.log.warning(
                "Error reading sensor, path: %s, error: %s." % (sSensorPath, 'invalid message'))
        f.close()
        if mytemp is not None:
            return int(mytemp[1])
        else:
            return None
    except Exception, exp:
        NodeControl.log.warning("Error reading sensor, path: %s, error: %s." % (sSensorPath, traceback.format_exc()))
        return None

def getCO2(NodeControl, sensorPort):
    """
    publisch CO2 value using mh_z19 sensor
    """
    ser = serial.Serial(sensorPort,
                          baudrate=9600,
                          bytesize=serial.EIGHTBITS,
                          parity=serial.PARITY_NONE,
                          stopbits=serial.STOPBITS_ONE,
                          timeout=1.0)
    while 1:
        result=ser.write("\xff\x01\x86\x00\x00\x00\x00\x00\x79")
        s=ser.read(9)
        if s[0] == "\xff" and s[1] == "\x86":
            NodeControl.MQTTPublish(sTopic="living/co2", sValue=str(ord(s[2])*256 + ord(s[3])), iQOS=0, bRetain=True)
        break



def checkDevices():
    # TODO
    # checks if all devices are found (does NOT check valid readings)
    allDevicesFound = False
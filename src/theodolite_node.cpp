//For ros
#include "ros/ros.h"
#include "std_msgs/String.h"

//For theodolite
#include "SsiCommand.h"
#include "SsiInstrument.h"
#include "VideoStreamingListener.h"
#include "ObservationListener.h"

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <array>
#include <fstream>
#include <iostream>
#include <vector>
#define ASIO_STANDALONE
#include <asio/thread_pool.hpp>
#include <asio/post.hpp>
#include<chrono>

//For LoRa antenna
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>

//Definition of variable for LoRa antenna
// #############################################
// #############################################

#define REG_FIFO                    0x00
#define REG_OPMODE                  0x01
#define REG_FIFO_ADDR_PTR           0x0D
#define REG_FIFO_TX_BASE_AD         0x0E
#define REG_FIFO_RX_BASE_AD         0x0F
#define REG_RX_NB_BYTES             0x13
#define REG_FIFO_RX_CURRENT_ADDR    0x10
#define REG_IRQ_FLAGS               0x12
#define REG_DIO_MAPPING_1           0x40
#define REG_DIO_MAPPING_2           0x41
#define REG_MODEM_CONFIG            0x1D
#define REG_MODEM_CONFIG2           0x1E
#define REG_MODEM_CONFIG3           0x26
#define REG_SYMB_TIMEOUT_LSB  		0x1F
#define REG_PKT_SNR_VALUE			0x19
#define REG_PAYLOAD_LENGTH          0x22
#define REG_IRQ_FLAGS_MASK          0x11
#define REG_MAX_PAYLOAD_LENGTH 		0x23
#define REG_HOP_PERIOD              0x24
#define REG_SYNC_WORD				0x39
#define REG_VERSION	  				0x42

#define PAYLOAD_LENGTH              0x40

// LOW NOISE AMPLIFIER
#define REG_LNA                     0x0C
#define LNA_MAX_GAIN                0x23
#define LNA_OFF_GAIN                0x00
#define LNA_LOW_GAIN		    	0x20

#define RegDioMapping1                             0x40 // common
#define RegDioMapping2                             0x41 // common

#define RegPaConfig                                0x09 // common
#define RegPaRamp                                  0x0A // common
#define RegPaDac                                   0x5A // common

#define SX72_MC2_FSK                0x00
#define SX72_MC2_SF7                0x70
#define SX72_MC2_SF8                0x80
#define SX72_MC2_SF9                0x90
#define SX72_MC2_SF10               0xA0
#define SX72_MC2_SF11               0xB0
#define SX72_MC2_SF12               0xC0

#define SX72_MC1_LOW_DATA_RATE_OPTIMIZE  0x01 // mandated for SF11 and SF12

// sx1276 RegModemConfig1
#define SX1276_MC1_BW_125                0x70
#define SX1276_MC1_BW_250                0x80
#define SX1276_MC1_BW_500                0x90
#define SX1276_MC1_CR_4_5            0x02
#define SX1276_MC1_CR_4_6            0x04
#define SX1276_MC1_CR_4_7            0x06
#define SX1276_MC1_CR_4_8            0x08

#define SX1276_MC1_IMPLICIT_HEADER_MODE_ON    0x01

// sx1276 RegModemConfig2
#define SX1276_MC2_RX_PAYLOAD_CRCON        0x04

// sx1276 RegModemConfig3
#define SX1276_MC3_LOW_DATA_RATE_OPTIMIZE  0x08
#define SX1276_MC3_AGCAUTO                 0x04

// preamble for lora networks (nibbles swapped)
#define LORA_MAC_PREAMBLE                  0x34

#define RXLORA_RXMODE_RSSI_REG_MODEM_CONFIG1 0x0A
#ifdef LMIC_SX1276
#define RXLORA_RXMODE_RSSI_REG_MODEM_CONFIG2 0x70
#elif LMIC_SX1272
#define RXLORA_RXMODE_RSSI_REG_MODEM_CONFIG2 0x74
#endif

// FRF
#define        REG_FRF_MSB              0x06
#define        REG_FRF_MID              0x07
#define        REG_FRF_LSB              0x08

#define        FRF_MSB                  0xD9 // 868.1 Mhz
#define        FRF_MID                  0x06
#define        FRF_LSB                  0x66

// ----------------------------------------
// Constants for radio registers
#define OPMODE_LORA      0x80
#define OPMODE_MASK      0x07
#define OPMODE_SLEEP     0x00
#define OPMODE_STANDBY   0x01
#define OPMODE_FSTX      0x02
#define OPMODE_TX        0x03
#define OPMODE_FSRX      0x04
#define OPMODE_RX        0x05
#define OPMODE_RX_SINGLE 0x06
#define OPMODE_CAD       0x07

// ----------------------------------------
// Bits masking the corresponding IRQs from the radio
#define IRQ_LORA_RXTOUT_MASK 0x80
#define IRQ_LORA_RXDONE_MASK 0x40
#define IRQ_LORA_CRCERR_MASK 0x20
#define IRQ_LORA_HEADER_MASK 0x10
#define IRQ_LORA_TXDONE_MASK 0x08
#define IRQ_LORA_CDDONE_MASK 0x04
#define IRQ_LORA_FHSSCH_MASK 0x02
#define IRQ_LORA_CDDETD_MASK 0x01

// DIO function mappings                D0D1D2D3
#define MAP_DIO0_LORA_RXDONE   0x00  // 00------
#define MAP_DIO0_LORA_TXDONE   0x40  // 01------
#define MAP_DIO1_LORA_RXTOUT   0x00  // --00----
#define MAP_DIO1_LORA_NOP      0x30  // --11----
#define MAP_DIO2_LORA_NOP      0xC0  // ----11--

// #############################################
// #############################################
//

// Define some type for LoRa antenna function
typedef bool boolean;
typedef unsigned char byte;
//Variable to receive the message transmitted
char message[256];
byte hello[32] = "HELLO";
byte receivedbytes;
// Channel of communication
static const int CHANNEL = 0;
// Option set for LoRa antenna
bool sx1272 = true;
//Different types of configuration for antenna
enum sf_t { SF7=7, SF8, SF9, SF10, SF11, SF12 };
// SX1272 - Raspberry connections
int ssPin = 6;
int dio0  = 7;
int RST   = 0;
// Set spreading factor (SF7 - SF12)
sf_t sf = SF7;
// Set center frequency
uint32_t  freq = 868100000; // in Mhz! (868.1)
// Data of theodolite
double HA;          //Horizontal angle
double VA;          //Vertical angle
double Dist;        //Distance
double Time;        //Time
int error_theodolite;   //flag for error
int theodolite_number = 0;
bool received_data = false;
bool break_iterator = false;

//Fonctions of LoRa antenna
// #############################################
// #############################################
//

void die(const char *s)
{
    perror(s);
    exit(1);
}

void selectreceiver()
{
    digitalWrite(ssPin, LOW);
}

void unselectreceiver()
{
    digitalWrite(ssPin, HIGH);
}

byte readReg(byte addr)
{
    unsigned char spibuf[2];

    selectreceiver();
    spibuf[0] = addr & 0x7F;
    spibuf[1] = 0x00;
    wiringPiSPIDataRW(CHANNEL, spibuf, 2);
    unselectreceiver();

    return spibuf[1];
}

void writeReg(byte addr, byte value)
{
    unsigned char spibuf[2];

    spibuf[0] = addr | 0x80;
    spibuf[1] = value;
    selectreceiver();
    wiringPiSPIDataRW(CHANNEL, spibuf, 2);

    unselectreceiver();
}

static void opmode (uint8_t mode) {
    writeReg(REG_OPMODE, (readReg(REG_OPMODE) & ~OPMODE_MASK) | mode);
}

static void opmodeLora() {
    uint8_t u = OPMODE_LORA;
    if (sx1272 == false)
        u |= 0x8;   // TBD: sx1276 high freq
    writeReg(REG_OPMODE, u);
}


void SetupLoRa()
{
    
    digitalWrite(RST, HIGH);
    delay(100);
    digitalWrite(RST, LOW);
    delay(100);

    byte version = readReg(REG_VERSION);

    if (version == 0x22) {
        // sx1272
        printf("SX1272 detected, starting.\n");
        sx1272 = true;
    } else {
        // sx1276?
        digitalWrite(RST, LOW);
        delay(100);
        digitalWrite(RST, HIGH);
        delay(100);
        version = readReg(REG_VERSION);
        if (version == 0x12) {
            // sx1276
            printf("SX1276 detected, starting.\n");
            sx1272 = false;
        } else {
            printf("Unrecognized transceiver.\n");
            //printf("Version: 0x%x\n",version);
            exit(1);
        }
    }

    opmode(OPMODE_SLEEP);

    // set frequency
    uint64_t frf = ((uint64_t)freq << 19) / 32000000;
    writeReg(REG_FRF_MSB, (uint8_t)(frf>>16) );
    writeReg(REG_FRF_MID, (uint8_t)(frf>> 8) );
    writeReg(REG_FRF_LSB, (uint8_t)(frf>> 0) );

    writeReg(REG_SYNC_WORD, 0x34); // LoRaWAN public sync word

    if (sx1272) {
        if (sf == SF11 || sf == SF12) {
            writeReg(REG_MODEM_CONFIG,0x0B);
        } else {
            writeReg(REG_MODEM_CONFIG,0x0A);
        }
        writeReg(REG_MODEM_CONFIG2,(sf<<4) | 0x04);
    } else {
        if (sf == SF11 || sf == SF12) {
            writeReg(REG_MODEM_CONFIG3,0x0C);
        } else {
            writeReg(REG_MODEM_CONFIG3,0x04);
        }
        writeReg(REG_MODEM_CONFIG,0x72);
        writeReg(REG_MODEM_CONFIG2,(sf<<4) | 0x04);
    }

    if (sf == SF10 || sf == SF11 || sf == SF12) {
        writeReg(REG_SYMB_TIMEOUT_LSB,0x05);
    } else {
        writeReg(REG_SYMB_TIMEOUT_LSB,0x08);
    }
    writeReg(REG_MAX_PAYLOAD_LENGTH,0x80);
    writeReg(REG_PAYLOAD_LENGTH,PAYLOAD_LENGTH);
    writeReg(REG_HOP_PERIOD,0x00);
    writeReg(REG_FIFO_ADDR_PTR, readReg(REG_FIFO_RX_BASE_AD));

    writeReg(REG_LNA, LNA_MAX_GAIN);

}

static void configPower (int8_t pw) {
    if (sx1272 == false) {
        // no boost used for now
        if(pw >= 17) {
            pw = 15;
        } else if(pw < 2) {
            pw = 2;
        }
        // check board type for BOOST pin
        writeReg(RegPaConfig, (uint8_t)(0x80|(pw&0xf)));
        writeReg(RegPaDac, readReg(RegPaDac)|0x4);

    } else {
        // set PA config (2-17 dBm using PA_BOOST)
        if(pw > 17) {
            pw = 17;
        } else if(pw < 2) {
            pw = 2;
        }
        writeReg(RegPaConfig, (uint8_t)(0x80|(pw-2)));
    }
}


static void writeBuf(byte addr, byte *value, byte len) {                                                       
    unsigned char spibuf[256];                                                                          
    spibuf[0] = addr | 0x80;                                                                            
    for (int i = 0; i < len; i++) {                                                                         
        spibuf[i + 1] = value[i];                                                                       
    }                                                                                                   
    selectreceiver();                                                                                   
    wiringPiSPIDataRW(CHANNEL, spibuf, len + 1);                                                        
    unselectreceiver();                                                                                 
}

void txlora(byte *frame, byte datalen) {

    // set the IRQ mapping DIO0=TxDone DIO1=NOP DIO2=NOP
    writeReg(RegDioMapping1, MAP_DIO0_LORA_TXDONE|MAP_DIO1_LORA_NOP|MAP_DIO2_LORA_NOP);
    // clear all radio IRQ flags
    writeReg(REG_IRQ_FLAGS, 0xFF);
    
    // mask all IRQs but TxDone
    //writeReg(REG_IRQ_FLAGS_MASK, ~IRQ_LORA_TXDONE_MASK);

    // initialize the payload size and address pointers
    writeReg(REG_FIFO_TX_BASE_AD, 0x00);
    writeReg(REG_FIFO_ADDR_PTR, 0x00);
    writeReg(REG_PAYLOAD_LENGTH, datalen);

    // download buffer to the radio FIFO
    writeBuf(REG_FIFO, frame, datalen);
    // now we actually start the transmission

    clock_t start, end;
    double cpu_time_used = 0.0;

    start = clock();
    opmode(OPMODE_TX);

    while(!(digitalRead(dio0) == 1))
    {
        delay(1);
    }
    
    end = clock(); 
    cpu_time_used = ((double)(end-start)) / CLOCKS_PER_SEC;
    printf ("Done sending in %.6lf s\n", cpu_time_used);
}

boolean receive(char *payload) {
    // clear rxDone
    writeReg(REG_IRQ_FLAGS, 0x40);

    int irqflags = readReg(REG_IRQ_FLAGS);

    //  payload crc: 0x20
    if((irqflags & 0x20) == 0x20)
    {
        printf("CRC error\n");
        writeReg(REG_IRQ_FLAGS, 0x20);
        return false;
    } else {

        byte currentAddr = readReg(REG_FIFO_RX_CURRENT_ADDR);
        byte receivedCount = readReg(REG_RX_NB_BYTES);
        receivedbytes = receivedCount;

        writeReg(REG_FIFO_ADDR_PTR, currentAddr);

        for(int i = 0; i < receivedCount; i++)
        {
            payload[i] = (char)readReg(REG_FIFO);
        }
    }
    return true;
}

void receivepacket(int theodolite_number) {

    long int SNR;
    int rssicorr;

    if(digitalRead(dio0) == 1)
    {
        if(receive(message)) {

            byte value = readReg(REG_PKT_SNR_VALUE);
            if( value & 0x80 ) // The SNR sign bit is 1
            {
                // Invert and divide by 4
                value = ( ( ~value + 1 ) & 0xFF ) >> 2;
                SNR = -value;
            }
            else
            {
                // Divide by 4
                SNR = ( value & 0xFF ) >> 2;
            }
            
            if (sx1272) {
                rssicorr = 139;
            } else {
                rssicorr = 157;
            }

            if((int)receivedbytes >= 2)
            {
                if(message[0]=='t' && message[1]==('0' + theodolite_number))
                {
                    printf("Packet RSSI: %d, ", readReg(0x1A)-rssicorr);
                    printf("RSSI: %d, ", readReg(0x1B)-rssicorr);
                    printf("SNR: %li, ", SNR);
                    printf("Length: %i", (int)receivedbytes);
                    printf("\n");
                    printf("Payload: %s\n", message);
                    message[0] = '\0';

                    received_data = true;
                }
            }
     
        } // received a message

    } // dio0=1
}

void General_setup_lora()
{
    wiringPiSetup () ;
    pinMode(ssPin, OUTPUT);
    pinMode(dio0, INPUT);
    pinMode(RST, OUTPUT);

    wiringPiSPISetup(CHANNEL, 500000);

    SetupLoRa();	
    opmodeLora();
}

void Config_rx_mode()
{
    // clear the irq
    writeReg(REG_IRQ_FLAGS, 0xFF);
    // set the IRQ mapping DIO0=RxDone DIO1=NOP DIO2=NOP
    writeReg(RegDioMapping1, MAP_DIO0_LORA_TXDONE | MAP_DIO1_LORA_NOP | MAP_DIO2_LORA_NOP);
    opmode(OPMODE_RX);
}

void Config_tx_mode()
{
    opmode(OPMODE_STANDBY);
}

void Received_data_check()
{
    while (received_data == false)
    {
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        while(1)
        {
            receivepacket(theodolite_number);
            delay(1);
            
            if(std::chrono::steady_clock::now() - start > std::chrono::milliseconds(200)) 
                    break;
        }
    }

    received_data = false;
}

//Fonction for theodolite
extern int ssi_output(const char* fmt, ...);

/////******************************************************************////////////
/////******************************************************************////////////
/////******************************************************************////////////

int main(int argc, char **argv)
{
	// Set up ROS.
    ros::init(argc, argv, "theodolite_node");
    ros::NodeHandle n;

    //while(ros::ok())
    {
        //Get parameters
        //Theodolite number (to differentiate data if many theodolites target one prism)
        n.getParam("/theodolite_node/theodolite_number", theodolite_number);
        //Number of target prism
        int target_prism = 0;
        n.getParam("/theodolite_node/target_prism", target_prism); 
        //Number of measurements decided   
        int number_of_measurements_choice = 10;
        n.getParam("/theodolite_node/number_of_measurments", number_of_measurements_choice);    
        bool use_lora = false;
        n.getParam("/theodolite_node/use_lora", use_lora);
        bool show_data = true;
        n.getParam("/theodolite_node/show_data", show_data);
        bool test_lora = false;
        n.getParam("/theodolite_node/test_lora", test_lora);
        int test_rate = 10;
        n.getParam("/theodolite_node/test_rate", test_rate);
        ros::Rate loop_rate(test_rate);
    
        printf("\n");

        //Initialize the connection with LoRa antenna if needed
        if(use_lora or test_lora)
        {
            printf("Starting LoRa antenna\n");

            General_setup_lora();
            printf("------------------\n");
        }

		// If no test with fake data
		if(!test_lora)
        {       
        
			if(theodolite_number < 1 or theodolite_number > 8)
			{
          		printf("Error in setting of theodolite number! Number should be between 1 and 8 \n");
            	break_iterator = true;
			}

			if(target_prism > 8 or target_prism < 1)
			{
				std::cout << "Wrong target prism number! Change the value to begin. Should be between 1 and 8" << std::endl;
            	break_iterator = true;
			}

			if(number_of_measurements_choice < 0)
			{
				std::cout << "Wrong number of measurements! Change the value to begin. Should be 0 or higher" << std::endl;
				break_iterator = true;
			}

			std::cout << "Target prism acquired is: " << target_prism << std::endl;
			if(number_of_measurements_choice != 0)
				std::cout << "Number of measurements decided is: " << number_of_measurements_choice << std::endl;
			else
				std::cout << "Number of measurements decided is infinite !" << std::endl;
                      
			//Load driver of the theodolite
			SsiInstrument& instrument = SsiInstrument::GetInstrument();
			instrument.LoadDriver();

			std::cout << "Loaded driver" << std::endl;	
			std::cout << "Connecting..." << std::endl;

			//Connect to the theodolite
			int err = instrument.Connect();
			if(err)	
			{
				std::cout << "Error during connection: " << err << std::endl;
                          
				//Disconnect driver of theodolite
				instrument.FreeDriver();

				std::cout << "Unloaded driver" << std::endl;
				std::cout << "Terminating program" << std::endl;
	            break_iterator = true;
			}

			if(break_iterator == false)
			{

				std::cout << "Intrument connected" << std::endl;
			              
				//Select Multitrack mode with the proper prism number
				instrument.Target(SsiInstrument::MODE_MULTITRACK, target_prism);
				std::shared_ptr<ObservationListener> observation_listener = std::make_shared<ObservationListener>();

				//Begin tracking of prism
				if(instrument.Tracking(true, observation_listener.get()) == 0)
				{
					//Variable to detect new measurments
					int number_of_measurements_new = 0;   //New number of measurments
					int number_of_measurements_old = 0;   //Old number of measurments
		                          
					std::cout << "Started measuring" << std::endl;

					if(number_of_measurements_choice == 0)  //Case we want not stop measurements
					{
						while(true)
						{
							try
							{
								if(use_lora)
								{
									Config_rx_mode();
									printf("------------------\n");
									Received_data_check();
									Config_tx_mode();
								}
								else
								{
									usleep(30);  //Time to wait a new measurement (frequency of measurements is around 2.5Hz)
								}    
                              
								number_of_measurements_new = observation_listener->getSizeVector();     //Get number of measurements stored
								if(number_of_measurements_new > number_of_measurements_old)         //If a new one is detected
								{

									HA = (observation_listener->getObservations())[0][number_of_measurements_new-1];
									VA = (observation_listener->getObservations())[1][number_of_measurements_new-1];
									Dist = (observation_listener->getObservations())[2][number_of_measurements_new-1];
									Time = (observation_listener->getObservations())[3][number_of_measurements_new-1];
									error_theodolite = (observation_listener->getObservations())[4][number_of_measurements_new-1];
									number_of_measurements_old = number_of_measurements_new;

	  								if(show_data)
									{
										//Print data of measurement
										std::cout << number_of_measurements_new << " measurements taken" << std::endl;
										std::cout << "HORIZONTAL_ANGLE_VECTOR: " << HA << std::endl;
										std::cout << "VERTICAL_ANGLE_VECTOR: " << VA << std::endl;
										std::cout << "DISTANCE_VECTOR: " << Dist << std::endl;
										std::cout << "TIMESTAMP_VECTOR: " << Time << std::endl;
										std::cout << "ERROR: " << error_theodolite << std::endl;
									}

									//Send data to robot
									if(use_lora)
									{										
								   		printf("------------------\n");
										std::string data = std::to_string(theodolite_number) + ";" + std::to_string(HA) + ";" + std::to_string(VA) + ";" + std::to_string(Dist) + ";" + std::to_string(Time) + ";";
										unsigned char *send_message = new unsigned char[data.length()+1];
										strcpy((char *)send_message,data.c_str());
										txlora(send_message, strlen((char *)send_message));

									 	delay(60); //20 is the minimum to send 2 bytes
									}

								}
							}
							catch(std::exception& e)
							{
								printf("%s\n", e.what());
							}
						}
					}
					else
					{
						while(number_of_measurements_new <= number_of_measurements_choice)      //Case we want a limited number of measurements
						{
							try
							{
								if(use_lora)
								{
									Config_rx_mode();
									printf("------------------\n");
									Received_data_check();
									Config_tx_mode();
								}
								else
								{
									usleep(30);  //Time to wait a new measurement (frequency of measurements is around 2.5Hz)
								}    
                              
								number_of_measurements_new = observation_listener->getSizeVector();     //Get number of measurements stored
								if(number_of_measurements_new > number_of_measurements_old)         //If a new one is detected
								{

									HA = (observation_listener->getObservations())[0][number_of_measurements_new-1];
									VA = (observation_listener->getObservations())[1][number_of_measurements_new-1];
									Dist = (observation_listener->getObservations())[2][number_of_measurements_new-1];
									Time = (observation_listener->getObservations())[3][number_of_measurements_new-1];
									error_theodolite = (observation_listener->getObservations())[4][number_of_measurements_new-1];
									number_of_measurements_old = number_of_measurements_new;

	  								if(show_data)
									{
										//Print data of measurement
										std::cout << number_of_measurements_new << " measurements taken" << std::endl;
										std::cout << "HORIZONTAL_ANGLE_VECTOR: " << HA << std::endl;
										std::cout << "VERTICAL_ANGLE_VECTOR: " << VA << std::endl;
										std::cout << "DISTANCE_VECTOR: " << Dist << std::endl;
										std::cout << "TIMESTAMP_VECTOR: " << Time << std::endl;
										std::cout << "ERROR: " << error_theodolite << std::endl;
									}

									//Send data to robot
									if(use_lora)
									{										
								   		printf("------------------\n");
										std::string data = std::to_string(theodolite_number) + ";" + std::to_string(HA) + ";" + std::to_string(VA) + ";" + std::to_string(Dist) + ";" + std::to_string(Time) + ";";
										unsigned char *send_message = new unsigned char[data.length()+1];
										strcpy((char *)send_message,data.c_str());
										txlora(send_message, strlen((char *)send_message));

									 	delay(60); //20 is the minimum to send 2 bytes
									}

								}
							}
							catch(std::exception& e)
							{
								printf("%s\n", e.what());
							}
						}
					}
					//Stop measurement when it's finished
					std::cout << "Stopped measuring" << std::endl;
					instrument.Tracking(false, observation_listener.get());

					//Save data in file if asked
					bool save_measurements = false;
					n.getParam("/theodolite_node/save_measurements", save_measurements); 
					if(save_measurements)
					{
						std::string file_measurements;
						n.getParam("/theodolite_node/file_measurements", file_measurements);
						std::cout << "Save measurements in " << file_measurements << std::endl;
						observation_listener->saveFile(file_measurements, 10);
					}
				}
				else
				{
					std::cout << "Error in starting tracking" << std::endl;
				}

				//Disconnect of the theodolite and remove driver
				std::cout << "Disconnecting..." << std::endl;
				instrument.FreeDriver();
				std::cout << "Unloaded driver" << std::endl;
				std::cout << "Terminating program" << std::endl;
			}
		}
		else
		{
			while(ros::ok())
			{
		    	Config_rx_mode();

		     	printf("------------------\n");

		     	Received_data_check();

		    	Config_tx_mode();

		   		printf("------------------\n");

		    	strncpy((char *)hello, "1;2.00000;5.00000;6.00000;1.59733e+09;" , sizeof(hello));
		    	txlora(hello, strlen((char *)hello));

		     	delay(60); //20 is the minimum to send 2 bytes
		     	ros::spinOnce();
		    }
		}
    }

	return 0;
}

int ssi_output(const char* fmt, ...)
{
	return 0;
}

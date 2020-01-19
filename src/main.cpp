#include <SPI.h>
#include <Ethernet.h>
#include <IRremote.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 
    0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE};
IPAddress ip(10, 18, 0, 48);

char cmd[15];
char param1[15];
boolean systemArmed = true;

typedef struct
{
  int screenDown;
  int projectorOn;
  int currentInput;
} SYSTEM_STATE;

#define STRING_BUFFER_SIZE 128
char buffer[STRING_BUFFER_SIZE];
#define MESSAGES_BUFFER_SIZE 32
char * messages[MESSAGES_BUFFER_SIZE]; // split list of http path parts
char emptystring[] = ""; // used to initialize messages

EthernetServer server(80);
IRsend irsend;

SYSTEM_STATE projector = {-1, -1, -1};

#define upPin 8
#define downPin 7

#define on	0x4CB340BF
#define off	0x4CB3748B
#define USER1	0x4CB36C93
#define USER2	0x4CB3A659
#define USER3	0x4CB36699
#define BRIGHTNESS	0x4CB3827D
#define CONTRAST	0x4CB342BD
#define MODE	0x4CB3A05F
#define KEYSTONE	0x4CB3E01F
#define ASPECT	0x4CB326D9
#define THREED	0x4CB3916E
#define MUTE	0x4CB34AB5
#define DynamicBlack	0x4CB322DD
#define SLEEPTIMER	0x4CB3C639
#define ENTER	0x4CB3F00F
#define MENU	0x4CB3708F
#define UPVOLUME	0x4CB38877
#define LTSOURCE	0x4CB308F7
#define RTRESYNC	0x4CB348B7
#define DNVOLUME	0x4CB328D7
#define HDMI1	0x4CB36897
#define HDMI2	0x4CB30CF3

void setup()
{
  pinMode(upPin, OUTPUT);
  pinMode(downPin, OUTPUT);
  digitalWrite(upPin, LOW);
  digitalWrite(downPin, LOW);
  
  Serial.begin(9600);
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.print(F("server is at "));
  Serial.println(Ethernet.localIP());
  pinMode(5, OUTPUT);
  digitalWrite(5, LOW);
}

void send404(EthernetClient client)
{
  client.println(F("HTTP/1.1 404 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connnection: close"));
     client.println();
  client.println(F("<!DOCTYPE HTML>"));
  client.println(F("<html><body>404</body></html>"));
}

void sendStatus(EthernetClient client)
{
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Connnection: close"));
      client.println();
      
  client.print(F("{\"ProjectorState\":"));
  if (projector.screenDown)
  {
    client.print(F("true"));
      }
  else
  {
    client.print(F("false"));
      }
     
  client.print(F(", \"ScreenState\":"));
  if (projector.projectorOn)
  {
    client.print(F("true"));
      }
  else
  {
    client.print(F("false"));
      }
  client.print(F("}"));
}

// command list
// replaces string literals that live in ram with flash
// so MAKE_COMMAND(foo); lets you replace "foo" with CMD_foo
// I'd like PROGMEM in here but that doesn't seem to work directly
// (I'd have to make reference thing that copies from flash to ram buffer)
#define MAKE_COMMAND(name) const char CMD_##name[] = #name
MAKE_COMMAND(help);
MAKE_COMMAND(status);
MAKE_COMMAND(on);
MAKE_COMMAND(off);
MAKE_COMMAND(user1);
MAKE_COMMAND(user2);
MAKE_COMMAND(user3);
MAKE_COMMAND(brightness);
MAKE_COMMAND(contrast);
MAKE_COMMAND(mode);
MAKE_COMMAND(keystone);
MAKE_COMMAND(aspect);
MAKE_COMMAND(3d);
MAKE_COMMAND(dynamicblack);
MAKE_COMMAND(sleeptimer);
MAKE_COMMAND(enter);
MAKE_COMMAND(menu);
MAKE_COMMAND(upvolume);
MAKE_COMMAND(dnvolume);
MAKE_COMMAND(ltsource);
MAKE_COMMAND(rtresync);
MAKE_COMMAND(hdmi1);
MAKE_COMMAND(hdmi2);
MAKE_COMMAND(screendown);
MAKE_COMMAND(screenup);
MAKE_COMMAND(mute);
 
void sendHelp(EthernetClient client)
{

   // Send a standard http response header
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Connnection: close"));
   client.println();

  client.println(F("Available Commands:"));
  Serial.println(CMD_help);
  client.println(CMD_help);
  client.println(CMD_status);
  client.println(CMD_on);
  client.println(CMD_off);
  client.println(CMD_user1);
  client.println(CMD_user2);
  client.println(CMD_user3);
  client.println(CMD_brightness);
  client.println(CMD_contrast);
  client.println(CMD_mode);
  client.println(CMD_keystone);
  client.println(CMD_aspect);
  client.println(CMD_3d);
  client.println(CMD_dynamicblack);
  client.println(CMD_sleeptimer);
  client.println(CMD_enter);
  client.println(CMD_menu);
  client.println(CMD_upvolume);
  client.println(CMD_dnvolume);
  client.println(CMD_ltsource);
  client.println(CMD_rtresync);
  client.println(CMD_hdmi1);
  client.println(CMD_hdmi2);
  client.println(CMD_screendown);
  client.println(CMD_screenup);
  client.println(CMD_mute);
}

/**
 * Parse the string and return an array which contains all path segments
 */
void parse(char *str)
{
  for (int i = 0; i < MESSAGES_BUFFER_SIZE; i++) {
    messages[i] = emptystring;
  }
	char *p;
	p = strtok(str, " ");
	unsigned int i = 0;
  while (p != NULL)
  {
	  p = strtok(NULL, "/");
	  char *sp;
	  boolean last = false;
	  sp = strchr(p, ' ');
    if (sp != NULL)
    {
            *sp++ = '\0';
	    last = true;
	  }
	  messages[i] = p;
	  i++;
    if (last)
    {
      break;
    }
    if (i +1 >= MESSAGES_BUFFER_SIZE) 
    { 
	    break;
	  }
	}

	messages[i] = '\0';
}

void setScreenState(bool down)
{
  if (down)
  {
    projector.screenDown = 1;
    digitalWrite(downPin, HIGH);
    delay(1000);
    digitalWrite(downPin, LOW);
  }
  else
  {
    projector.screenDown = 0;
    digitalWrite(upPin, HIGH);
    delay(1000);
    digitalWrite(upPin, LOW);
  }
}

void handleCommand(const EthernetClient &client, char *cmd, char *param)
{
  if (strcmp(cmd, "help") == 0)
  {
      sendHelp(client);
      
    Serial.println(F("help"));
    }  
  else if (strcmp(cmd, CMD_on) == 0)
  {
      irsend.sendNEC(on, 32);
    Serial.println(F("on"));
      projector.projectorOn = 1;
      setScreenState(true);
      sendStatus(client);
    } 
    
  else if (strcmp(cmd, CMD_off) == 0)
  {
      irsend.sendNEC(off, 32);
      delay(900);
      irsend.sendNEC(off, 32);
    Serial.println(F("off"));
      projector.projectorOn = 0;
      setScreenState(false);
      sendStatus(client);
    }
  else if (strcmp(cmd, CMD_user1) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(USER1, 32);
      Serial.println(cmd);
    }  
  else if (strcmp(cmd, CMD_user2) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(USER2, 32);
      Serial.println(cmd);
    }    
  else if (strcmp(cmd, CMD_user3) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(USER3, 32);
      Serial.println(cmd);
    }      
  else if (strcmp(cmd, CMD_brightness) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(BRIGHTNESS, 32);
      Serial.println(cmd);
    } 
  else if (strcmp(cmd, CMD_contrast) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(CONTRAST, 32);
      Serial.println(cmd);
    } 
  else if (strcmp(cmd, CMD_mode) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(MODE, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_keystone) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(KEYSTONE, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_aspect) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(ASPECT, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_3d) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(THREED, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_mute) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(MUTE, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_dynamicblack) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(DynamicBlack, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_sleeptimer) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(SLEEPTIMER, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_enter) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(ENTER, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_menu) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(MENU, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_upvolume) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(UPVOLUME, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_dnvolume) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(DNVOLUME, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_ltsource) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(LTSOURCE, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_rtresync) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(RTRESYNC, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_hdmi1) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(HDMI1, 32);
      Serial.println(cmd);
    }
  else if (strcmp(cmd, CMD_hdmi2) == 0)
  {
      sendStatus(client);
      irsend.sendNEC(HDMI2, 32);
      Serial.println(cmd);
    } 
  else if (strcmp(cmd, CMD_status) == 0)
  {
      sendStatus(client);
    }
  else if (strcmp(cmd, CMD_screendown) == 0)
  {
      setScreenState(true);
      projector.screenDown = 1;
    }
  else if (strcmp(cmd, CMD_screenup) == 0)
  {
      irsend.sendNEC(off, 32);
      delay(900);
      irsend.sendNEC(off, 32);
    Serial.println(F("off"));
      projector.projectorOn = 0;
      projector.screenDown = 0;
      setScreenState(false);
    }
  else
  {
      send404(client);
    }  
}

int countSegments(char *str)
{
  int p = 0;
  int count = 0;
  while (str[p] != '\0')
  {
    if (str[p] == '/')
    {
      count++;
    }
    p++;
  }
  // We don't want to count the / in 'HTTP/1.1'
  count--;
  return count;
}

void loop()
{
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client)
  {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected())
    {
      if (client.available())
      {
        
        char c;
        int bufindex = 0; // reset buffer
        buffer[0] = client.read();
        buffer[1] = client.read();
        bufindex = 2;
        // Read the first line to determin the request page
        while (buffer[bufindex - 2] != '\r' && buffer[bufindex - 1] != '\n')
        {
            // read full row and save it in buffer
            c = client.read();
          if (bufindex < STRING_BUFFER_SIZE)
          {
              buffer[bufindex] = c;
            }
            bufindex++;
        }
        // Clean buffer for next row
        bufindex = 0;
 
        // Parse the query string
        int nSegments = countSegments(buffer);
        parse(buffer);
 
        int i = 0;
        for (i = 0; i < nSegments; i++)
        {
          Serial.println(messages[i]);
        }

        if (c == '\n' && currentLineIsBlank)
        {
          handleCommand(client, messages[0], messages[1]);
          break;
        }

        if (c == '\n')
        {
          currentLineIsBlank = true;
        }
        else if (c != '\r')
        {
          currentLineIsBlank = false;
        }
      }
    }
    // Give the web browser time to receive the data
    delay(1);
    // Close the connection:
    client.stop();
    Serial.println(F("Client disonnected"));
  }
}

The ethernet library attached is modified slightly to allow pin 8 to be the slave select pin (dedicated) for ethernet communications instead of the hard coded pin 10.

The ethernet card used is a cheap Chinese made card from Ebay loaded with the Wiznet 5100 chip connected to a long range freakduino 900mhz model.  Digital pins 13-10, 4, and analog pins 2,3 are bent up… the middle spi pins cover ethernet comms and pin 8 is jumped from the bent pin 10 on the ethernet card for ss using this library and code to change 10 to 8 manually.


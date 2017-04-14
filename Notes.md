Here's a list of running notes for the ssd1351

Big ass problems:
Well it looks like the init commands were not actually being sent... Apparently after each COMMAMD, sometimes DATA needs to be sent

In the SSD1331 case, DC is low for ALL of the commands.. but in SSD1351 case, we need to bring DC high when sending the second part of the value... 

aside from this, need to send data to ram command. before it allows gddr writes

looks like the rest of the nanoscreen code is adaptable and should work

what you might want to do is, get the length of the pixel buffer array to make sure it matches with your calculations

 i guess we need to initialize the display correctly with the correct 16 bit init sequence before the nanoscreen will work
 
 stop gap is maybe my pytbon app will initialize the display properly and then the C program will take over and write to the display properly? the problem is when the column and address need to be reset... maybe just brute force the col row reset? 
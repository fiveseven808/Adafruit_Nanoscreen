Here's a list of running notes for the ssd1351

Big ass problems:
Well it looks like the init commands were not actually being sent... Apparently after each COMMAMD, sometimes DATA needs to be sent

In the SSD1331 case, DC is low for ALL of the commands.. but in SSD1351 case, we need to bring DC high when sending the second part of the value... 

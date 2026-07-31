IBM Enhanced Graphics Adapter
Contents

Description ............................................. 1
    Major Components .................................. 3
    Modes of Operation ............................... 5
    Basic Operations .................................. 8
    Registers ......................................... 12
Programming Considerations ............................. 62
    Programming the Registers ......................... 62
    RAM Loadable Character Generator ................. 69
    Creating a 512 Character Set ..................... 70
    Creating an 80 by 43 Alphanumeric Mode .......... 71
    Vertical Interrupt Feature ....................... 72
    Creating a Split Screen .......................... 73
    Compatibility Issues ............................ 74
Interface ............................................. 76
    Feature Connector ................................ 76
Specifications ........................................ 79
    System Board Switches ........................... 79
    Configuration Switches .......................... 80
    Direct Drive Connector ........................... 83
    Light Pen Interface .............................. 84
    Jumper Descriptions ............................. 85
Logic Diagrams ........................................ 87
BIOS Listing .......................................... 103
    Vectors with Special Meanings ................... 103

Index .................................................. Index-1
Description

The IBM Enhanced Graphics Adapter (EGA) is a graphics controller that supports both color and monochrome direct drive displays in a variety of modes. In addition to the direct drive port, a light pen interface is provided. Advanced features on the adapter include bit-mapped graphics in four planes and a RAM (Random Access Memory) loadable character generator. Design features in the hardware substantially reduce the software overhead for many graphics functions.

The Enhanced Graphics Adapter provides Basic Input Output System (BIOS) support for both alphanumeric (A/N) modes and all-points-addressable (APA) graphics modes, including all modes supported by the Monochrome Display Adapter and the Color/Graphics Monitor Adapter. Other modes provide APA 640x350 pel graphics support for the IBM Monochrome Display, full 16 color support in both 320x200 pel and 640x200 pel resolutions for the IBM Color Display, and both A/N and APA support with resolution of 640x350 for the IBM Enhanced Color Display. In alphanumeric modes, characters are formed from one of two ROM (Read Only Memory) character generators on the adapter. One character generator defines 7x9 characters in a 9x14 character box. For Enhanced Color Display support, the 9x14 character set is modified to provide an 8x14 character set. The second character generator defines 7x7 characters in an 8x8 character box. These generators contain dot patterns for 256 different characters. The character sets are identical to those provided by the IBM Monochrome Display Adapter and the IBM Color/Graphics Monitor Adapter.

The adapter contains 64K bytes of storage configured as four 16K byte bit planes. Memory expansion options are available to expand the adapter memory to 128K bytes or 256K bytes.

The adapter is packaged on a single 13-1/8 inch (333.50 mm) card. The direct drive port is a right-angle mounted connector at the rear of the adapter and extends through the rear panel of the system unit. Also on the card are five large scale integration (LSI) modules custom designed for this controller.
Located on the adapter is a feature connector that provides access to internal functions through a 32-pin berg connector. A separate 64-pin connector provides an interface for graphics memory expansion.

The following is a block diagram of the Enhanced Graphics Adapter:

![Block diagram of the Enhanced Graphics Adapter](page_180_370_1047_563.png)

Enhanced Graphics Adapter Block Diagram
Major Components

CRT Controller

The CRT (Cathode Ray Tube) Controller (CRTC) generates horizontal and vertical synchronous timings, addressing for the regenerative buffer, cursor and underline timings, and refresh addressing for the dynamic RAMs.

Sequencer

The Sequencer generates basic memory timings for the dynamic RAMs and the character clock for controlling regenerative memory fetches. It allows the processor to access memory during active display intervals by inserting dedicated processor memory cycles periodically between the display memory cycles. Map mask registers are available to protect entire memory maps from being changed.

Graphics Controller

The Graphics Controller directs the data from the memory to the attribute controller and the processor. In graphics modes, memory data is sent in serialized form to the attribute chip. In alpha modes the memory data is sent in parallel form, bypassing the graphics controller. The graphics controller formats the data for compatible modes and provides color comparators for use in color painting modes. Other hardware facilities allow the processor to write 32 bits in a single memory cycle, (8 bits per plane) for quick color presetting of the display areas, and additional logic allows the processor to write data to the display on non-byte boundaries.

Attribute Controller

The Attribute Controller provides a color palette of 16 colors, each of which may be specified separately. Six color outputs are
available for driving a display. Blinking and underlining are controlled by this chip. This chip takes data from the display memory and formats it for display on the CRT screen.

Display Buffer

The display buffer on the adapter consists of 64K bytes of dynamic read/write memory configured as four 16K byte video bit planes. Two options are available for expanding the graphics memory. The Graphics Memory Expansion Card plugs into the memory expansion connector on the adapter, and adds one bank of 16K to each of the four bit planes, increasing the graphics memory to 128K bytes. The expansion card also provides DIP sockets for further memory expansion. Populating the DIP sockets with the Graphics Memory Module Kit adds two additional 16K banks to each bit plane, bringing the graphics memory to its maximum of 256K bytes.

The address of the display buffer can be changed to remain compatible with other video cards and application software. Four locations are provided. The buffer can be configured at segment address hex A0000 for a length of 128K bytes, at hex A0000 for a length of 64K bytes, at hex B0000 for a length of 32K bytes, or at hex B8000 for a length of 32K bytes.

BIOS

A read-only memory (ROM) Basic Input Output System (BIOS) module on the adapter is linked to the system BIOS. This ROM BIOS contains character generators and control code and is mapped into the processor address at hex C0000 for a length of 16K bytes.

Support Logic

The logic on the card surrounding the LSI modules supports the modules and creates latch buses for the CRT controller, the
processor, and character generator. Two clock sources (14 MHz and 16 MHz) provide the dot rate. The clock is multiplexed under processor I/O control. Four I/O registers also resident on the card are not part of the LSI devices.

Modes of Operation

IBM Color Display

The following table describes the modes supported by BIOS on the IBM Color Display:

<table>
  <tr>
    <th>MODE #</th>
    <th>TYPE</th>
    <th>COLORS</th>
    <th>ALPHA FORMAT</th>
    <th>BUFFER START</th>
    <th>BOX SIZE</th>
    <th>MAX. PAGES</th>
    <th>RESOLUTION</th>
  </tr>
  <tr>
    <td>0</td>
    <td>A/N</td>
    <td>16</td>
    <td>40x25</td>
    <td>B8000</td>
    <td>8x8</td>
    <td>8</td>
    <td>320x200</td>
  </tr>
  <tr>
    <td>1</td>
    <td>A/N</td>
    <td>16</td>
    <td>40x25</td>
    <td>B8000</td>
    <td>8x8</td>
    <td>8</td>
    <td>320x200</td>
  </tr>
  <tr>
    <td>2</td>
    <td>A/N</td>
    <td>16</td>
    <td>80x25</td>
    <td>B8000</td>
    <td>8x8</td>
    <td>8</td>
    <td>640x200</td>
  </tr>
  <tr>
    <td>3</td>
    <td>A/N</td>
    <td>16</td>
    <td>80x25</td>
    <td>B8000</td>
    <td>8x8</td>
    <td>8</td>
    <td>640x200</td>
  </tr>
  <tr>
    <td>4</td>
    <td>APA</td>
    <td>4</td>
    <td>40x25</td>
    <td>B8000</td>
    <td>8x8</td>
    <td>1</td>
    <td>320x200</td>
  </tr>
  <tr>
    <td>5</td>
    <td>APA</td>
    <td>4</td>
    <td>40x25</td>
    <td>B8000</td>
    <td>8x8</td>
    <td>1</td>
    <td>320x200</td>
  </tr>
  <tr>
    <td>6</td>
    <td>APA</td>
    <td>2</td>
    <td>80x25</td>
    <td>B8000</td>
    <td>8x8</td>
    <td>1</td>
    <td>640x200</td>
  </tr>
  <tr>
    <td>D</td>
    <td>APA</td>
    <td>16</td>
    <td>40x25</td>
    <td>A0000</td>
    <td>8x8</td>
    <td>2/4/8</td>
    <td>320x200</td>
  </tr>
  <tr>
    <td>E</td>
    <td>APA</td>
    <td>16</td>
    <td>80x25</td>
    <td>A0000</td>
    <td>8x8</td>
    <td>1/2/4</td>
    <td>640x200</td>
  </tr>
</table>

Modes 0 through 6 emulate the support provided by the IBM Color/Graphics monitor Adapter.

Modes 0,2 and 5 are identical to modes 1,3 and 4 respectively at the adapter's direct drive interface.

The Maximum Pages fields for modes D and E indicate the number of pages supported when 64K, 128K or 256K bytes of graphics memory is installed, respectively.
IBM Monochrome Display

The following table describes the modes supported by BIOS on the IBM Monochrome Display.

<table>
  <tr>
    <th>MODE #</th>
    <th>TYPE</th>
    <th>COLORS</th>
    <th>ALPHA FORMAT</th>
    <th>BUFFER START</th>
    <th>BOX SIZE</th>
    <th>MAX. PAGES</th>
    <th>RESOLUTION</th>
  </tr>
  <tr>
    <td>7</td>
    <td>A/N</td>
    <td>4</td>
    <td>80x25</td>
    <td>B0000</td>
    <td>9x14</td>
    <td>8</td>
    <td>720x350</td>
  </tr>
  <tr>
    <td>F</td>
    <td>APA</td>
    <td>4</td>
    <td>80x25</td>
    <td>A0000</td>
    <td>8x14</td>
    <td>1/2</td>
    <td>640x350</td>
  </tr>
</table>

Mode 7 emulates the support provided by the IBM Monochrome Display Adapter.

IBM Enhanced Color Display

The Enhanced Graphics Adapter supports attachment of the IBM Enhanced Color Display. The IBM Enhanced Color Display is capable of running at the standard television frequency of 15.75 KHz as well as running 21.85 KHz. The table below summarizes the characteristics of the IBM Enhanced Color Display:

<table>
  <tr>
    <th>Parameter</th>
    <th>TV Frequency</th>
    <th>High Resolution</th>
  </tr>
  <tr>
    <td>Horiz Scan Rate</td>
    <td>15.75 KHz.</td>
    <td>21.85 KHz.</td>
  </tr>
  <tr>
    <td>Vertical Scan Rate</td>
    <td>60 Hz.</td>
    <td>60 Hz.</td>
  </tr>
  <tr>
    <td>Video Bandwidth</td>
    <td>14.318 MHz.</td>
    <td>16.257 MHz.</td>
  </tr>
  <tr>
    <td>Displayable Colors</td>
    <td>16 Maximum</td>
    <td>16 or 64</td>
  </tr>
  <tr>
    <td>Character Size</td>
    <td>7 by 7 Pels</td>
    <td>7 by 9 Pels</td>
  </tr>
  <tr>
    <td>Character Box Size</td>
    <td>8 by 8 Pels</td>
    <td>8 by 14 Pels</td>
  </tr>
  <tr>
    <td>Maximum Resolution</td>
    <td>640x200 Pels</td>
    <td>640 by 350 Pels</td>
  </tr>
  <tr>
    <td>Alphanumeric Modes</td>
    <td>0,1,2,3</td>
    <td>0,1,2,3</td>
  </tr>
  <tr>
    <td>Graphics Modes</td>
    <td>4,5,6,D,E</td>
    <td>10</td>
  </tr>
</table>

In the television frequency mode, the IBM Enhanced Color Display displays information identical in color and resolution to the IBM Color Display.

In the high resolution mode, the adapter provides enhanced alphanumeric character support. This enhanced alphanumeric support consists of transforming the 8 by 8 character box into an 8 by 14 character box, and providing 16 colors out of a palette of
64 possible display colors. Display colors are changed by altering the programming of the color palette registers in the Attribute Controller. In alphanumeric modes, any 16 of 64 colors are displayable. the screen resolution is 320x350 for modes 0 and 1, and 640x350 for modes 2 and 3.

The resolution displayed on the IBM Enhanced Color Display is selected by the switch settings on the Enhanced Graphics Adapter.

The Enhanced Color Display is compatible with all modes listed for the IBM Color Display. the following table describes additional modes supported by BIOS for the IBM Enhanced Color Display:

<table>
  <tr>
    <th>MODE #</th>
    <th>TYPE</th>
    <th>COLORS</th>
    <th>ALPHA FORMAT</th>
    <th>BUFFER START</th>
    <th>BOX SIZE</th>
    <th>MAX. PAGES</th>
    <th>RESOLUTION</th>
  </tr>
  <tr>
    <td>0*</td>
    <td>A/N</td>
    <td>16/64</td>
    <td>40x25</td>
    <td>B8000</td>
    <td>8x14</td>
    <td>8</td>
    <td>320x350</td>
  </tr>
  <tr>
    <td>1*</td>
    <td>A/N</td>
    <td>16/64</td>
    <td>40x25</td>
    <td>B8000</td>
    <td>8x14</td>
    <td>8</td>
    <td>320x350</td>
  </tr>
  <tr>
    <td>2*</td>
    <td>A/N</td>
    <td>16/64</td>
    <td>80x25</td>
    <td>B8000</td>
    <td>8x14</td>
    <td>8</td>
    <td>640x350</td>
  </tr>
  <tr>
    <td>3*</td>
    <td>A/N</td>
    <td>16/64</td>
    <td>80x25</td>
    <td>B8000</td>
    <td>8x14</td>
    <td>8</td>
    <td>640x350</td>
  </tr>
  <tr>
    <td>10*</td>
    <td>APA</td>
    <td>4/16<br>16/64</td>
    <td>80x25</td>
    <td>A0000</td>
    <td>8x14</td>
    <td>1/2</td>
    <td>640x350</td>
  </tr>
</table>

* Note that modes 0, 1, 2, and 3, are also listed for IBM Color Display support. BIOS provides enhanced support for these modes when an Enhanced Color Display is attached.

The values in the "COLORS" field indicate 16 colors of a 64 color palette or 4 colors of a sixteen color palette.

In mode 10, The dual values for the "COLORS" field and the "MAX. PAGES" field indicate the support provided when 64K or when greater than 64K of graphics memory is installed, respectively.
Basic Operations

Alphanumeric Modes

The data format for alphanumeric modes on the Enhanced Graphics Adapter is the same as the data format on the IBM Color/Graphics Monitor Adapter and the IBM Monochrome Display Adapter. As an added function, bit three of the attribute byte may be redefined by the Character Map Select register to act as a switch between character sets. This gives the programmer access to 512 characters at one time. This function is valid only when memory has been expanded to 128K bytes or more.

When an alphanumeric mode is selected, the BIOS transfers character patterns from the ROM to bit plane 2. The processor stores the character data in bit plane 0, and the attribute data in bit plane 1. The programmer can view bit planes 0 and 1 as a single buffer in alphanumeric modes. The CRTC generates sequential addresses, and fetches one character code byte and one attribute byte at a time. The character code and row scan count address bit plane 2, which contains the character generators. The appropriate dot patterns are then sent to the palette in the attribute chip, where color is assigned according to the attribute data.

Graphics Modes

320x200 Two and Four Color Graphics (Modes 4 and 5)

Addressing, mapping and data format are the same as the 320x200 pel mode of the Color/Graphics Monitor Adapter. The display buffer is configured at hex B8000. Bit image data is stored in bit planes 0 and 1.

640x200 Two Color Graphics (Mode 6)

Addressing, mapping and data format are the same as the 640x200 pel black and white mode of the Color/Graphics
Monitor Adapter. The display buffer is configured at hex B8000. Bit image data is stored in bit plane 0.

640x350 Monochrome Graphics (Mode F )

This mode supports graphics on the IBM Monochrome Display with the following attributes: black, video, blinking video, and intensified video. Resolution of 640x350 requires 56K bytes to support four attributes. By chaining maps 0 and 1, then maps 2 and 3 together, two 32K bit planes can be formed. This chaining is done only when necessary (less than 128K of graphics memory). The first map is the video bit plane, and the second map is the intensity bit plane. Both planes reside at hex address A0000.

Two bits, one from each bit plane, define one picture element (pel) on the screen. The bit definitions for the pels are given in the following table. The video bit plane is denoted by C0 and the Intensity Bit Plane is denoted by C2.

<table>
  <tr>
    <th>C2</th>
    <th>C0</th>
    <th>Pixel Color</th>
    <th>Valid Attributes</th>
  </tr>
  <tr>
    <td>0</td>
    <td>0</td>
    <td>Black</td>
    <td>0</td>
  </tr>
  <tr>
    <td>0</td>
    <td>1</td>
    <td>Video</td>
    <td>3</td>
  </tr>
  <tr>
    <td>1</td>
    <td>0</td>
    <td>Blinking Video</td>
    <td>C</td>
  </tr>
  <tr>
    <td>1</td>
    <td>1</td>
    <td>Intensified Video</td>
    <td>F</td>
  </tr>
</table>

The byte organization in memory is sequential. The first eight pels on the screen are defined by the contents of memory in location A000:0H, the second eight pels by location A000:1H, and so on. The first pel within any one byte is defined by bit 7 in the byte. The last pel within the byte is defined by bit 0 in the byte.

Monochrome graphics works in odd/even mode, which means that even CPU addresses go into even bit planes and odd CPU addresses go into odd bit planes. Since both bit planes reside at address A0000, the user must select which plane or planes he desires to update. This is accomplished by the map mask register of the sequencer. (See the table above for valid attributes).
16/64 Color Graphics Modes (Mode 10)

These modes support graphics in 16 colors on either a medium or high resolution monitor. The memory in these modes consists of using all four bit planes. Each bit plane represents a color as shown below. The bit planes are denoted as C0,C1,C2 and C3 respectively.

C0 = Blue Pels
C1 = Green Pels
C2 = Red Pels
C3 = Intensified Pels

Four bits (one from each plane) define one pel on the screen. The color combinations are illustrated in the following table:

<table>
  <tr>
    <th>I</th>
    <th>R</th>
    <th>G</th>
    <th>B</th>
    <th>Color</th>
  </tr>
  <tr><td>0</td><td>0</td><td>0</td><td>0</td><td>Black</td></tr>
  <tr><td>0</td><td>0</td><td>0</td><td>1</td><td>Blue</td></tr>
  <tr><td>0</td><td>0</td><td>1</td><td>0</td><td>Green</td></tr>
  <tr><td>0</td><td>0</td><td>1</td><td>1</td><td>Cyan</td></tr>
  <tr><td>0</td><td>1</td><td>0</td><td>0</td><td>Red</td></tr>
  <tr><td>0</td><td>1</td><td>0</td><td>1</td><td>Magenta</td></tr>
  <tr><td>0</td><td>1</td><td>1</td><td>0</td><td>Brown</td></tr>
  <tr><td>0</td><td>1</td><td>1</td><td>1</td><td>White</td></tr>
  <tr><td>1</td><td>0</td><td>0</td><td>0</td><td>Dark Gray</td></tr>
  <tr><td>1</td><td>0</td><td>0</td><td>1</td><td>Light Blue</td></tr>
  <tr><td>1</td><td>0</td><td>1</td><td>0</td><td>Light Green</td></tr>
  <tr><td>1</td><td>0</td><td>1</td><td>1</td><td>Light Cyan</td></tr>
  <tr><td>1</td><td>1</td><td>0</td><td>0</td><td>Light Red</td></tr>
  <tr><td>1</td><td>1</td><td>0</td><td>1</td><td>Light Magenta</td></tr>
  <tr><td>1</td><td>1</td><td>1</td><td>0</td><td>Yellow</td></tr>
  <tr><td>1</td><td>1</td><td>1</td><td>1</td><td>Intensified White</td></tr>
</table>

The display buffer resides at address A0000. The map mask register of the sequencer is used to select any or all of the bit planes to be updated when a memory write to the display buffer is executed by the CPU.

Color Mapping

The Enhanced Graphics Adapter supports 640x350 Graphics for both the IBM Monochrome and the IBM Enhanced Color
Displays. Four color capability is supported on the EGA without the Graphics Memory Expansion Card (base 64 KB), and sixteen colors are supported when the Graphics Memory Expansion Card is installed on the adapter (128 KB or above). This section describes the differences in the colors displayed depending upon the graphics memory available. Note that colors 0H, 1H, 4H, and 7H map directly regardless of the graphics memory available.

<table>
  <tr>
    <th>Character Attribute</th>
    <th>Monochrome</th>
    <th>Mode 10H 64KB</th>
    <th>Mode 10H >64KB</th>
  </tr>
  <tr>
    <td>00H*</td>
    <td>Black</td>
    <td>Black</td>
    <td>Black</td>
  </tr>
  <tr>
    <td>01H*</td>
    <td>Video</td>
    <td>Blue</td>
    <td>Blue</td>
  </tr>
  <tr>
    <td>02H</td>
    <td>Black</td>
    <td>Black</td>
    <td>Green</td>
  </tr>
  <tr>
    <td>03H</td>
    <td>Video</td>
    <td>Blue</td>
    <td>Cyan</td>
  </tr>
  <tr>
    <td>04H*</td>
    <td>Blinking</td>
    <td>Red</td>
    <td>Red</td>
  </tr>
  <tr>
    <td>05H</td>
    <td>Intensified</td>
    <td>White</td>
    <td>Magenta</td>
  </tr>
  <tr>
    <td>06H</td>
    <td>Blinking</td>
    <td>Red</td>
    <td>Brown</td>
  </tr>
  <tr>
    <td>07H*</td>
    <td>Intensified</td>
    <td>White</td>
    <td>White</td>
  </tr>
  <tr>
    <td>08H</td>
    <td>Black</td>
    <td>Black</td>
    <td>Dark Gray</td>
  </tr>
  <tr>
    <td>09H</td>
    <td>Video</td>
    <td>Blue</td>
    <td>Light Blue</td>
  </tr>
  <tr>
    <td>0AH</td>
    <td>Black</td>
    <td>Black</td>
    <td>Light Green</td>
  </tr>
  <tr>
    <td>0BH</td>
    <td>Video</td>
    <td>Blue</td>
    <td>Light Cyan</td>
  </tr>
  <tr>
    <td>OCH</td>
    <td>Blinking</td>
    <td>Red</td>
    <td>Light Red</td>
  </tr>
  <tr>
    <td>ODH</td>
    <td>Intensified</td>
    <td>White</td>
    <td>Light Magenta</td>
  </tr>
  <tr>
    <td>0EH</td>
    <td>Blinking</td>
    <td>Red</td>
    <td>Yellow</td>
  </tr>
  <tr>
    <td>OFH</td>
    <td>Intensified</td>
    <td>White</td>
    <td>Intensified White</td>
  </tr>
</table>

* Graphics character attributes which map directly regardless of the graphics memory available.
Registers

External Registers

This section contains descriptions of the registers of the Enhanced Graphics Adapter that are not contained in an LSI device.

<table>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
  </tr>
  <tr>
    <td>Miscellaneous Output Register</td>
    <td>3C2</td>
    <td>-</td>
  </tr>
  <tr>
    <td>Feature Control Register<br>Input Status Register 0</td>
    <td>3?A<br>3C2</td>
    <td>-<br>-</td>
  </tr>
  <tr>
    <td>Input Status Register 1</td>
    <td>3?2</td>
    <td>-</td>
  </tr>
  <tr>
    <td colspan="2">? = B in Monochrome Modes<br>? = D in Color Modes</td>
    <td></td>
  </tr>
</table>

Miscellaneous Output Register

This is a write-only register. The processor output port address is hex 3C2. A hardware reset causes all bits to reset to zero.

Miscellaneous Output Register Format

![Bit location diagram for Miscellaneous Output Register](page_668_624_639_277.png)

Bit 0      3BX/3DX CRTC I/O Address—This bit maps the CRTC I/O addresses for IBM Monochrome or Color/Graphics Monitor Adapter emulation. A logical 0 sets CRTC addresses to 3BX and Input Status Register 1's address to 3BA for Monochrome emulation. A logical 1 sets CRTC
addresses to 3DX and Input Status Register 1's address to 3DA for Color/Graphics Monitor Adapter emulation.

Bit 1    Enable RAM—A logical 0 disables RAM from the processor; a logical 1 enables RAM to respond at addresses designated by the Control Data Select value programmed into the Graphics Controllers.

Bit 2-Bit 3    Clock Select—These two bits select the clock source according to the following table:

<table>
  <tr>
    <th>Bits</th>
    <th>3 2</th>
  </tr>
  <tr>
    <td>0 0-</td>
    <td>Selects 14 MHz clock from the processor I/O channel</td>
  </tr>
  <tr>
    <td>0 1-</td>
    <td>Selects 16 MHz clock on-board oscillator</td>
  </tr>
  <tr>
    <td>1 0-</td>
    <td>Selects external clock source from the feature connector.</td>
  </tr>
  <tr>
    <td>1 1-</td>
    <td>Not used</td>
  </tr>
</table>

Bit 4    Disable Internal Video Drivers—A logical 0 activates internal video drivers; a logical 1 disables internal video drivers. When the internal video drivers are disabled, the source of the direct drive color output becomes the feature connector direct drive outputs.

Bit 5    Page Bit For Odd/Even—Selects between two 64K pages of memory when in the Odd/Even modes (0,1,2,3,7). A logical 0 selects the low page of memory; a logical 1 selects the high page of memory.

Bit 6    Horizontal Retrace Polarity—A logical 0 selects positive horizontal retrace; a logical 1 selects negative horizontal retrace.

Bit 7    Vertical Retrace Polarity—A logical 0 selects positive vertical retrace; a logical 1 selects
negative vertical retrace. The IBM Monochrome display requires a negative vertical retrace polarity.

Feature Control Register

This is a write-only register. The processor output register is hex 3BA or 3DA.

<table>
  <tr>
    <th colspan="8">Feature Control Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <td>7</td>
    <td>6</td>
    <td>5</td>
    <td>4</td>
    <td>3</td>
    <td>2</td>
    <td>1</td>
    <td>0</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Feature Control Bit 0</td>
    <td>Feature Control Bit 1</td>
    <td>Reserved</td>
    <td>Not Used</td>
  </tr>
</table>

Bits 0 and 1 Feature Control Bits—These bits are used to convey information to the feature connector. The output of these bits goes to the FEAT 0 (pin 19) and FEAT 1 (pin 17) of the feature connector.

Input Status Register Zero

This is a read-only register. The processor input port address is hex 3C2.
Input Status Register Zero Format

Bit 7 6 5 4 3 2 1 0
Not Used
Switch Sense
Reserved
Reserved
CRT Interrupt

Bit 4    Switch Sense—When set to 1, this bit allows the processor to read the four configuration switches on the board. The setting of the CLKSEL field determines which switch is being read. The switch configuration can be determined by reading byte 40:88H in RAM.

    Bit 3: Switch 4 ; Logical 0 = switch closed
    Bit 2: Switch 3 ; Logical 0 = switch closed
    Bit 1: Switch 2 ; Logical 0 = switch closed
    Bit 0: Switch 1 ; Logical 0 = switch closed

Bits 5 and 6    Feature Code—These bits are input from the Feat (0) and Feat (1) pins on the feature connector.

Bit 7    CRT Interrupt—A logical 1 indicates video is being displayed on the CRT screen; a logical 0 indicates that vertical retrace is occurring.

Input Status Register One

This is a read-only register. The processor port address is hex 3BA or hex 3DA.
Bit 0    Display Enable—Logical 0 indicates the CRT raster is in a horizontal or vertical retrace interval. This bit is the real time status of the display enable signal. Some programs use this status bit to restrict screen updates to inactive display intervals. The Enhanced Graphics Adapter does not require the CPU to update the screen buffer during inactive display intervals to avoid glitches in the display image.

Bit 1    Light Pen Strobe—A logical 0 indicates that the light pen trigger has not been set; a logical 1 indicates that the light pen trigger has been set.

Bit 2    Light Pen Switch—A logical 0 indicates that the light pen switch is closed; a logical 1 indicates that the light pen switch is open.

Bit 3    Vertical Retrace—A logical 0 indicates that video information is being displayed on the CRT screen; a logical 1 indicates the CRT is in a vertical retrace interval. This bit can be programmed to interrupt the processor on interrupt level 2 at the start of the vertical retrace. This is done through bits 4 and 5 of the Vertical Retrace End Register of the CRTC.

Bits 4 and 5    Diagnostic Usage—These bits are selectively connected to two of the six color outputs of the
Attribute Controller. The Color Plane Enable register controls the multiplexer for the video wiring. The following table illustrates the combinations available and the color output wiring.

<table>
  <tr>
    <th rowspan="2">Color Plane Register</th>
    <th colspan="2">Input Status Register One</th>
  </tr>
  <tr>
    <th>Bit 5</th>
    <th>Bit 4</th>
  </tr>
  <tr>
    <td>Bit 5 Bit 4</td>
    <td>Bit 5</td>
    <td>Bit 4</td>
  </tr>
  <tr>
    <td>0 0</td>
    <td>Red</td>
    <td>Blue</td>
  </tr>
  <tr>
    <td>0 1</td>
    <td>Secondary Blue</td>
    <td>Green</td>
  </tr>
  <tr>
    <td>1 0</td>
    <td>Secondary Red</td>
    <td>Secondary Green</td>
  </tr>
  <tr>
    <td>1 1</td>
    <td>Not Used</td>
    <td>Not Used</td>
  </tr>
</table>
Sequencer Registers

<table>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
  </tr>
  <tr>
    <td>Address</td>
    <td>3C4</td>
    <td>-</td>
  </tr>
  <tr>
    <td>Reset</td>
    <td>3C5</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Clocking Mode</td>
    <td>3C5</td>
    <td>01</td>
  </tr>
  <tr>
    <td>Map Mask</td>
    <td>3C5</td>
    <td>02</td>
  </tr>
  <tr>
    <td>Character Map Select</td>
    <td>3C5</td>
    <td>03</td>
  </tr>
  <tr>
    <td>Memory Mode</td>
    <td>3C5</td>
    <td>04</td>
  </tr>
</table>

Sequencer Address Register

The Address Register is a pointer register located at address hex 3C4. This register is loaded with a binary value that points to the sequencer data register where data is to be written. This value is referred to as "Index" in the table above.

<table>
  <tr>
    <th colspan="8">Sequencer Address Register Format</th>
  </tr>
  <tr>
    <td>Bit</td>
    <td>7</td>
    <td>6</td>
    <td>5</td>
    <td>4</td>
    <td>3</td>
    <td>2</td>
    <td>1</td>
    <td>0</td>
    <td>Sequencer Address</td>
    <td>Not Used</td>
  </tr>
</table>

Bit 0–Bit 3    Sequencer Address Bits—A binary value pointing to the register where data is to be written.

Reset Register

This is a write-only register pointed to when the value in the address register is hex 00. The output port address for this register is hex 3C5.
Bit 0    Asynchronous Reset—A logical 0 commands the sequencer to asynchronous clear and halt. All outputs are placed in the high impedance state when this bit is a 0. A logical 1 commands the sequencer to run unless bit 1 is set to zero. Resetting the sequencer with this bit can cause data loss in the dynamic RAMs.

Bit 1    Synchronous Reset—A logical 0 commands the sequencer to synchronous clear and halt. Bits 1 and 0 must both be ones to allow the sequencer to operate. Reset the sequencer with this bit before changing the Clocking Mode Register, if memory contents are to be preserved.

Clocking Mode Register

This is a write-only register pointed to when the value in the address register is hex 01. The output port address for this register is hex 3C5.
Bit 0    8/9 Dot Clocks—A logical 0 directs the sequencer to generate character clocks 9 dots wide; a logical 1 directs the sequencer to generate character clocks 8 dots wide. Monochrome alphanumeric mode (07H) is the only mode that uses character clocks 9 dots wide. All other modes must use 8 dots per character clock.

Bit 1    Bandwidth—A logical 0 makes CRT memory cycles occur on 4 out of 5 available memory cycles; a logical 1 makes CRT memory cycles occur on 2 out of 5 available memory cycles. Medium resolution modes require less data to be fetched from the display buffer during the horizontal scan time. This allows the CPU greater access time to the display buffer. All high resolution modes must provide the CRTC with 4 out of 5 memory cycles in order to refresh the display image.

Bit 2    Shift Load—When set to 0, the video serializers are reloaded every character clock; when set to 1, the video serializers are loaded every other character clock. This mode is useful when 16 bits are fetched per cycle and chained together in the shift registers.

Bit 3    Dot Clock—A logical 0 selects normal dot clocks derived from the sequencer master clock input. When this bit is set to 1, the master clock will be divided by 2 to generate the dot clock. All the other timings will be stretched since they are derived from the dot clock. Dot clock divided by two is used for 320x200 modes (0, 1, 4, 5) to provide a pixel rate of 7 MHz, (9 MHz for mode D).

Map Mask Register

This is a write-only register pointed to when the value in the address register is hex 02. The output port address for this register is hex 3C5.
Bit 0–Bit 3    Map Mask—A logical 1 in bits 3 through 0 enables the processor to write to the corresponding maps 3 through 0. If this register is programmed with a value of 0FH, the CPU can perform a 32-bit write operation with only one memory cycle. This substantially reduces the overhead on the CPU during display update cycles in graphics modes. Data scrolling operations are also enhanced by setting this register to a value of 0FH and writing the display buffer address with the data stored in the CPU data latches. This is a read-modify-write operation. When odd/even modes are selected, maps 0 and 1 and maps 2 and 3 should have the same map mask value.

Character Map Select Register

This is a write-only register pointed to when the value in the address register is hex 03. The output port address for this register is 3C5.
Character Map Select Register Format

Bit 7 6 5 4 3 2 1 0
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |

    Character Map Select B
    Character Map Select A
    Not Used

Bit 0-Bit 1    Character Map Select B—Selects the map used to generate alpha characters when attribute bit 3 is a 0, according to the following table:

<table>
  <tr>
    <th>Bits<br>1 0</th>
    <th>Map Selected</th>
    <th>Table Location</th>
  </tr>
  <tr>
    <th>Value</th>
    <th></th>
    <th></th>
  </tr>
  <tr>
    <td>0 0</td>
    <td>0</td>
    <td>1st 8K of Plane 2 Bank 0</td>
  </tr>
  <tr>
    <td>0 1</td>
    <td>1</td>
    <td>2nd 8K of Plane 2 Bank 1</td>
  </tr>
  <tr>
    <td>1 0</td>
    <td>2</td>
    <td>3rd 8K of Plane 2 Bank 2</td>
  </tr>
  <tr>
    <td>1 1</td>
    <td>3</td>
    <td>4th 8K of Plane 2 Bank 3</td>
  </tr>
</table>

Bit 2-Bit 3    Character Map Select A—Selects the map used to generate alpha characters when attribute bit 3 is a 1, according to the following table:

<table>
  <tr>
    <th>Bits<br>3 2</th>
    <th>Map Selected</th>
    <th>Table Location</th>
  </tr>
  <tr>
    <th>Value</th>
    <th></th>
    <th></th>
  </tr>
  <tr>
    <td>0 0</td>
    <td>0</td>
    <td>1st 8K of Plane 2 Bank 0</td>
  </tr>
  <tr>
    <td>0 1</td>
    <td>1</td>
    <td>2nd 8K of Plane 2 Bank 1</td>
  </tr>
  <tr>
    <td>1 0</td>
    <td>2</td>
    <td>3rd 8K of Plane 2 Bank 2</td>
  </tr>
  <tr>
    <td>1 1</td>
    <td>3</td>
    <td>4th 8K of Plane 2 Bank 3</td>
  </tr>
</table>

In alphanumeric modes, bit 3 of the attribute byte normally has the function of turning the foreground intensity on or off. This bit however may be redefined as a switch between character sets. This function is enabled when there is a difference between the value in Character Map Select A and the value in Character Map Select B. Whenever these two values are the same, the character select function is disabled. The memory mode register bit 1 must be a 1 (indicates the memory extension card is installed in the unit) to enable this function; otherwise, bank 0 is always selected.
128K of graphics memory is required to support two character sets. 256K supports four character sets. Asynchronous reset clears this register to 0. This should be done only when the sequencer is reset.

Memory Mode Register

This is a write-only register pointed to when the value in the address register is hex 04. The processor output port address for this register is 3C5.

<table>
  <tr>
    <th colspan="8">Memory Mode Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Alpha</th>
    <th>Extended Memory</th>
    <th>Odd/Even</th>
    <th>Not Used</th>
  </tr>
</table>

Bit 0    Alpha—A logical 0 indicates that a non-alpha mode is active. A logical 1 indicates that alpha mode is active and enables the character generator map select function.

Bit 1    Extended Memory—A logical 0 indicates that the memory expansion card is not installed. A logical 1 indicates that the memory expansion card is installed and enables access to the extended memory through address bits 14 and 15.

Bit 2    Odd/Even—A logical 0 directs even processor addresses to access maps 0 and 2, while odd processor addresses access maps 1 and 3. A logical 1 causes processor addresses to sequentially access data within a bit map. The maps are accessed according to the value in the map mask register.
CRT Controller Registers

<table>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
  </tr>
  <tr><td>Address Register</td><td>3?4</td><td>-</td></tr>
  <tr><td>Horizontal Total</td><td>3?5</td><td>00</td></tr>
  <tr><td>Horizontal Display End</td><td>3?5</td><td>01</td></tr>
  <tr><td>Start Horizontal Blank</td><td>3?5</td><td>02</td></tr>
  <tr><td>End Horizontal Blank</td><td>3?5</td><td>03</td></tr>
  <tr><td>Start Horizontal Retrace</td><td>3?5</td><td>04</td></tr>
  <tr><td>End Horizontal Retrace</td><td>3?5</td><td>05</td></tr>
  <tr><td>Vertical Total</td><td>3?5</td><td>06</td></tr>
  <tr><td>Overflow</td><td>3?5</td><td>07</td></tr>
  <tr><td>Preset Row Scan</td><td>3?5</td><td>08</td></tr>
  <tr><td>Max Scan Line</td><td>3?5</td><td>09</td></tr>
  <tr><td>Cursor Start</td><td>3?5</td><td>0A</td></tr>
  <tr><td>Cursor End</td><td>3?5</td><td>0B</td></tr>
  <tr><td>Start Address High</td><td>3?5</td><td>0C</td></tr>
  <tr><td>Start Address Low</td><td>3?5</td><td>0D</td></tr>
  <tr><td>Cursor Location High</td><td>3?5</td><td>0E</td></tr>
  <tr><td>Cursor Location Low</td><td>3?5</td><td>0F</td></tr>
  <tr><td>Vertical Retrace Start</td><td>3?5</td><td>10</td></tr>
  <tr><td>Light Pen High</td><td>3?5</td><td>10</td></tr>
  <tr><td>Vertical Retrace End</td><td>3?5</td><td>11</td></tr>
  <tr><td>Light Pen Low</td><td>3?5</td><td>11</td></tr>
  <tr><td>Vertical Display End</td><td>3?5</td><td>12</td></tr>
  <tr><td>Offset</td><td>3?5</td><td>13</td></tr>
  <tr><td>Underline Location</td><td>3?5</td><td>14</td></tr>
  <tr><td>Start Vertical Blank</td><td>3?5</td><td>15</td></tr>
  <tr><td>End Vertical Blank</td><td>3?5</td><td>16</td></tr>
  <tr><td>Mode Control</td><td>3?5</td><td>17</td></tr>
  <tr><td>Line Compare</td><td>3?5</td><td>18</td></tr>
</table>

? = B in Monochrome Modes and D in Color Modes

CRT Controller Address Register

The Address register is a pointer register located at hex 3B4 or hex 3D4. If an IBM Monochrome Display is attached to the adapter, address 3B4 is used. If a color display is attached to the adapter, address 3D4 is used. This register is loaded with a binary value that points to the CRT Controller data register where data is to be written. This value is referred to as "Index" in the table above.
Bit 0–Bit 4    CRT Controller Address Bits—A binary value pointing to the CRT Controller register where data is to be written.

Horizontal Total Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 00. The processor output port address for this register is hex 3B5 or hex 3D5.

Bit 0–Bit 7    Horizontal Total—The total number of characters less 2.
Horizontal Display Enable End Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 01. The processor output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Horizontal Display Enable End Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Horizontal Display Enable End</th>
  </tr>
</table>

This register defines the length of the horizontal display enable signal. It determines the number of displayed character positions per horizontal line.

Bit 0–Bit 7    Horizontal display enable end —A value one less than the total number of displayed characters.

Start Horizontal Blanking Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 02. The processor output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Start Horizontal Blanking Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Start Vertical Blanking</th>
  </tr>
</table>

This register determines when the horizontal blanking output signal becomes active. The row scan address and underline scan line decode outputs are multiplexed on the memory address outputs and cursor outputs respectively during the blanking interval. These outputs are latched external to the CRT Controller with the falling edge of the BLANK output signal. The row scan address and underline signals remain on the output signals for one character count beyond the end of the blanking signal.
Bit 0-Bit 7    Start Horizontal Blanking—The horizontal blanking signal becomes active when the horizontal character counter reaches this value.

End Horizontal Blanking Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 03. The processor output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">End Horizontal Blanking Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th></th>
    <th>End Blanking</th>
    <th>Display Enable Skew Control</th>
    <th>Not Used</th>
  </tr>
</table>

This register determines when the horizontal blanking output signal becomes inactive. The row scan address and underline scan line decode outputs are multiplexed on the memory address outputs and the cursor outputs respectively during the blanking interval. These outputs are latched external to the CRT Controller with the falling edge of the BLANK output signal. The row scan address and underline signals remain on the output signals for one character count beyond the end of the blanking signal.

Bit 0-Bit 4   End Horizontal Blanking—A value equal to the five least significant bits of the horizontal character counter value at which time the horizontal blanking signal becomes inactive (logical 0). To obtain a blanking signal of width W, the following algorithm is used: Value of Start Blanking Register + Width of Blanking signal in character clock units = 5-bit result to be programmed into the End Horizontal Blanking Register.
Bit 5–Bit 6    Display Enable Skew Control—These two bits determine the amount of display enable skew. Display enable skew control is required to provide sufficient time for the CRT Controller to access the display buffer to obtain a character and attribute code, access the character generator font, and then go through the Horizontal Pel Panning Register in the Attribute Controller. Each access requires the display enable signal to be skewed one character clock unit so that the video output is in synchronization with the horizontal and vertical retrace signals. The bit values and amount of skew are shown in the following table:

Bits
6  5

0 0    Zero character clock skew
0 1    One character clock skew
1 0    Two character clock skew
1 1    Three character clock skew

Start Horizontal Retrace Pulse Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 04. The processor output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Start Horizontal Retrace Pulse Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Start Horizontal Retrace Pulse</th>
  </tr>
</table>

This register is used to center the screen horizontally, and to specify the character position at which the Horizontal Retrace Pulse becomes active.
Bit 0–Bit 7      Start Horizontal Retrace Pulse—The value programmed is a binary count of the character position number at which the signal becomes active.

End Horizontal Retrace Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 05. The processor output port address for this register is hex 3B5 or hex 3D5.

![Diagram showing the End Horizontal Retrace Register Format, with bits 7-0 labeled, and arrows indicating the positions for End Horizontal Retrace, Horizontal Retrace Delay, and Start Odd Memory Address](page_364_617_1002_246.png)

This register specifies the character position at which the Horizontal Retrace Pulse becomes inactive (logical 0).

Bit 0–Bit 4      End Horizontal Retrace—A value equal to the five least significant bits of the horizontal character counter value at which time the horizontal retrace signal becomes inactive (logical 0). To obtain a retrace signal of width W, the following algorithm is used: Value of Start Retrace Register + width of horizontal retrace signal in character clock units = 5-bit result to be programmed into the End Horizontal Retrace Register.

Bit 5–Bit 6      Horizontal Retrace Delay—These bits control the skew of the horizontal retrace signal. Binary 00 equals no Horizontal Retrace Delay. For some modes, it is necessary to provide a horizontal retrace signal that takes up the entire blanking interval. Some internal timings are generated by the falling edge of the horizontal retrace signal. To guarantee the signals are
latched properly, the retrace signal is started before the end of the display enable signal, and then skewed several character clock times to provide the proper screen centering.

Bit 7
Start Odd/Even Memory Address—This bit controls whether the first CRT memory address output after a horizontal retrace begins with an even or an odd address. A logical 0 selects even addresses; a logical 1 selects odd addresses. This bit is used for horizontal pel panning applications. Generally, this bit should be set to a logical 0.

Vertical Total Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 06. The processor output port address for this register is hex 3B5 or 3D5.

<table>
  <tr>
    <th colspan="8">Vertical Total Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Vertical Total</th>
  </tr>
</table>

Bit 0–Bit 7 Vertical Total—This is the low-order eight bits of a nine-bit register. The binary value represents the number of horizontal raster scans on the CRT screen, including vertical retrace. The value in this register determines the period of the vertical retrace signal. Bit 8 of this register is contained in the CRT Controller Overflow Register hex 07 bit 0.

CRT Controller Overflow Register

This is a write-only register pointed to when the value in the CRT Controller Address Register is hex 07. The processor output port address for this register is hex 3B5 or hex 3D5.
CRTC Overflow Register Format

Bit 7 6 5 4 3 2 1 0
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |

    → Vertical Total Bit 8
    → Vertical Display Enable End Bit 8
    → Vertical Retrace Start Bit 8
    → Start Vertical Blank Bit 8
    → Line Compare Bit 8
    → Cursor Location Bit 8
    → Not Used

Bit 0      Vertical Total—Bit 8 of the Vertical Total register (index hex 06).

Bit 1      Vertical Display Enable End—Bit 8 of the Vertical Display Enable End register (index hex 12).

Bit 2      Vertical Retrace Start—Bit 8 of the Vertical Retrace Start register (index hex 10).

Bit 3      Start Vertical Blank—Bit 8 of the Start Vertical Blank register (index hex 15).

Bit 4      Line Compare—Bit 8 of the Line Compare register (index hex 18).

Bit 5      Cursor Location—Bit 8 of the Cursor Location register (index hex 0A).

Preset Row Scan Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 08. The processor output port address for this register is hex 3B5 or hex 3D5.
Preset Row Scan Register Format

Bit 7 6 5 4 3 2 1 0
Starting Row Scan Count after a Vertical Retrace
Not Used

This register is used for pel scrolling.

Bit 0–Bit 4    Preset Row Scan (Pel Scrolling)—This register specifies the starting row scan count after a vertical retrace. The row scan counter increments each horizontal retrace time until a maximum row scan occurs. At maximum row scan compare time the row scan is cleared (not preset).

Maximum Scan Line Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 09. The processor output port address for this register is hex 3B5 or hex 3D5.

Maximum Scan Line Register Format

Bit 7 6 5 4 3 2 1 0
Maximum Scan Line
Not Used

Bit 0–Bit 4    Maximum Scan Line—This register specifies the number of scan lines per character row. The number to be programmed is the maximum row scan number minus one.

Cursor Start Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 0A. The processor output port
address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Cursor Start Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Row Scan Cursor Begins<br>Not Used</td>
  </tr>
</table>

Bit 0-Bit 4    Cursor Start—This register specifies the row scan of a character line where the cursor is to begin. The number programmed should be one less than the starting cursor row scan.

Cursor End Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 0B. The processor output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Cursor End Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Row Scan Cursor Ends<br>Cursor Skew Control<br>Not Used</td>
  </tr>
</table>

Bit 0-Bit 4    Cursor End—These bits specify the row scan where the cursor is to end.

Bit 5-Bit 6    Cursor Skew—These bits control the skew of the cursor signal.
Bits
6 5

0 0  Zero character clock skew
0 1  One character clock skew
1 0  Two character clock skew
1 1  Three character clock skew

Start Address High Register

This is a read/write register pointed to when the value in the CRT Controller address register is hex 0C. The processor input/output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Start Address High Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>High Order Start Address</th>
  </tr>
</table>

Bit 0–Bit 7  Start Address High—These are the high-order eight bits of the start address. The 16-bit value, from the high-order and low-order start address registers, is the first address after the vertical retrace on each screen refresh.

Start Address Low Register

This is a read/write register pointed to when the value in the CRT Controller address register is hex 0D. The processor input/output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Start Address Low Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Low Order Start Address</th>
  </tr>
</table>
Bit 0-Bit 7    Start Address Low—These are the low-order 8 bits of the start address.

Cursor Location High Register

This is a read/write register pointed to when the value in the CRT Controller address register is hex OE. The processor input/output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Cursor Location High Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>High Order Cursor Location</th>
  </tr>
</table>

Bit 0-Bit 7    Cursor Location High—These are the high-order 8 bits of the cursor location.

Cursor Location Low Register

This is a read/write register pointed to when the value in the CRT Controller address register is hex OF. The processor input/output port address for this register is hex 3B5 or Hex 3D5.

<table>
  <tr>
    <th colspan="8">Cursor Location Low Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Low Order Cursor Location</th>
  </tr>
</table>

Bit 0-Bit 7    Cursor Location Low—These are the low-order 8 bits of the cursor location.
Vertical Retrace Start Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 10. The processor output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Vertical Retrace Start Register Format</th>
  </tr>
  <tr>
    <th>Bit</th><th>7</th><th>6</th><th>5</th><th>4</th><th>3</th><th>2</th><th>1</th><th>0</th>
    <td>Low Order Vertical Retrace Pulse</td>
  </tr>
</table>

Bit 0–Bit 7  Vertical Retrace Start—This is the low-order 8 bits of the vertical retrace pulse start position programmed in horizontal scan lines. Bit 8 is in the overflow register location hex 07.

Light Pen High Register

This is a read-only register pointed to when the value in the CRT Controller address register is hex 10. The processor input port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Light Pen High Register Format</th>
  </tr>
  <tr>
    <th>Bit</th><th>7</th><th>6</th><th>5</th><th>4</th><th>3</th><th>2</th><th>1</th><th>0</th>
    <td>High Order Memory Address Counter</td>
  </tr>
</table>

Bit 0–Bit 7  Light Pen High—This is the high order 8 bits of the memory address counter at the time the light pen was triggered.

Vertical Retrace End Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 11. The processor output port
address for this register is hex 3B5 or hex 3D5.

![Vertical Retrace End Register Format diagram](page_184_120_1016_172.png)

Bit 0–Bit 3    Vertical Retrace End—These bits determine the horizontal scan count value when the vertical retrace output signal becomes inactive. The register is programmed in units of horizontal scan lines. To obtain a vertical retrace signal of width W, the following algorithm is used: Value of Start Vertical Retrace Register + width of vertical retrace signal in horizontal scan units = 4-bit result to be programmed into the End Horizontal Retrace Register.

Bit 4    Clear Vertical Interrupt—A logical 0 will clear a vertical interrupt.

Bit 5    Enable Vertical Interrupt—A logical 0 will enable vertical interrupt.

Light Pen Low Register

This is a read-only register pointed to when the value in the CRT Controller address register is hex 11. The processor input port address for this register is hex 3B5 or 3D5.
Bit 0–Bit 7    Light Pen Low—This is is the low-order 8 bits of the memory address counter at the time the light pen was triggered.

Vertical Display Enable End Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 12. The processor output port address for this register is hex 3B5 or hex 3D5.

Bit 0–Bit 7    Vertical Display Enable End—These are the low-order 8 bits of the vertical display enable end position. This address specifies which scan line ends the active video area of the screen. Bit 8 is in the overflow register location hex 07.

Offset Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 13. The processor output port address for this register is hex 3B5 or hex 3D5.
Offset Register Format

Bit 7 6 5 4 3 2 1 0
Logical line width of the screen

Bit 0–Bit 7 Offset—This register specifies the logical line width of the screen. The starting memory address for the next character row is larger than the current character row by this amount. The Offset Register is programmed with a word address. Depending upon the method of clocking the CRT Controller, this word address is either a word or double word address.

Underline Location Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 14. The processor output port address for this register is hex 3B5 or hex 3D5.

Underline Location Register Format

Bit 7 6 5 4 3 2 1 0
Horizontal row scan where underline will occur
Not Used

Bit 0–Bit 4 Underline Location—This register specifies the horizontal row scan on which underline will occur. The value programmed is one less than the scan line number desired.

Start Vertical Blanking Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 15. The processor output port
address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Start Vertical Blanking Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
  </tr>
  <tr>
    <td colspan="8"></td>
    <td>Start Vertical Blanking</td>
  </tr>
</table>

Bit 0–Bit 7   Start Vertical Blank—These are the low 8 bits of the horizontal scan line count, at which the vertical blanking signal becomes active. Bit 8 bit is in the overflow register hex 07.

End Vertical Blanking Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 16. The processor output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">End Vertical Blanking Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
  </tr>
  <tr>
    <td colspan="4"></td>
    <td colspan="4"></td>
    <td>End Vertical Blanking</td>
    <td>Not Used</td>
  </tr>
</table>

Bit 0–Bit 4   End Vertical Blank—This register specifies the horizontal scan count value when the vertical blank output signal becomes inactive. The register is programmed in units of horizontal scan lines. To obtain a vertical blank signal of width W, the following algorithm is used: Value of Start Vertical Blank Register + width of vertical blank signal in horizontal scan units = 5-bit result to be programmed into the End Vertical Blank Register.
Mode Control Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 17. The processor output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Mode Control Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>CMS 0</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Select Row Scan Counter</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Horizontal Retrace Select</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Count by Two</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Output Control</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Address Wrap</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Word/Byte Mode</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Hardware Reset</td>
  </tr>
</table>

Bit 0  Compatibility Mode Support— When this bit is a logical 0, the row scan address bit 0 is substituted for memory address bit 13 during active display time. A logical 1 enables memory address bit 13 to appear on the memory address output bit 13 signal of the CRT Controller. The CRT Controller used on the IBM Color/Graphics Monitor Adapter is the 6845. The 6845 has 128 horizontal scan line address capability. To obtain 640 by 200 graphics resolution, the CRTC was programmed for 100 horizontal scan lines with 2 row scan addresses per character row. Row scan address bit 0 became the most significant address bit to the display buffer. Successive scan lines of the display image were displaced in memory by 8K bytes. This bit allows compatibility with the 6845 and Color Graphics APA modes of operation.
Bit 1    Select Row Scan Counter—A logical 0 selects row scan counter bit 1 on MA 14 output pin. A logical 1 selects MA 14 counter bit on MA 14 output pin.

Bit 2    Horizontal Retrace Select—This bit selects Horizontal Retrace or Horizontal Retrace divided by 2 as the clock that controls the vertical timing counter. This bit can be used to effectively double the vertical resolution capability of the CRT Controller. The vertical counter has a maximum resolution of 512 scan lines due to the 9-bit wide Vertical Total Register. If the vertical counter is clocked with the horizontal retrace divided by 2 clock, then the vertical resolution is doubled to 1024 horizontal scan lines. A logical 0 selects HRTC and a logical 1 selects HRTC divided by 2.

Bit 3    Count By Two— When this bit is set to 0, the memory address counter is clocked with the character clock input. A logical 1 clocks the memory address counter with the character clock input divided by 2. This bit is used to create either a byte or word refresh address for the display buffer.

Bit 4    Output Control—A logical 0 enables the module output drivers. A logical 1 forces all outputs into high impedance state.

Bit 5    Address Wrap—This bit selects Memory Address counter bit MA 13 or bit MA 15, and it appears on the MA 0 output pin in the word address mode. If you are not in the word address mode, MA 0 counter output appears on the MA 0 output pin. A logical 1 selects MA 15. In odd/even mode, bit MA 13 should be selected when the 64K memory is installed on the board. Bit MA 15 should be selected when greater then 64K memory is installed. This function is used to implement Color Graphics Monitor Adapter compatibility.
Bit 6    Word Mode or Byte Mode—When this bit is a logical 0, the Word Mode shifts all memory address counter bits down one bit, and the most significant bit of the counter appears on the least significant bit of the memory address outputs. See table below for address output details. A logical 1 selects the Byte Address mode.

<table>
  <tr>
    <th colspan="3">Internal Memory Address Counter Wiring to the Output Multiplexer</th>
  </tr>
  <tr>
    <th>CRTC Out Pin</th>
    <th>Byte Address Mode</th>
    <th>Word Address Mode</th>
  </tr>
  <tr>
    <td>MA 0/RFA 0</td>
    <td>MA 0</td>
    <td>MA 15 or MA 13</td>
  </tr>
  <tr>
    <td>MA 1/RFA 1</td>
    <td>MA 1</td>
    <td>MA 0</td>
  </tr>
  <tr>
    <td>MA 2/RFA 2</td>
    <td>MA 2</td>
    <td>MA 1</td>
  </tr>
  <tr>
    <td>MA 3/RFA 3</td>
    <td>MA 3</td>
    <td>MA 2</td>
  </tr>
  <tr>
    <td>*</td>
    <td>*</td>
    <td>*</td>
  </tr>
  <tr>
    <td>*</td>
    <td>*</td>
    <td>*</td>
  </tr>
  <tr>
    <td>*</td>
    <td>*</td>
    <td>*</td>
  </tr>
  <tr>
    <td>MA 14/RS 3</td>
    <td>MA 14</td>
    <td>MA 13</td>
  </tr>
  <tr>
    <td>MA 15/RS 4</td>
    <td>MA 15</td>
    <td>MA 14</td>
  </tr>
</table>

Bit 7    Hardware Reset—A logical 0 forces horizontal and vertical retrace to clear. A logical 1 forces horizontal and vertical retrace to be enabled.

Line Compare Register

This is a write-only register pointed to when the value in the CRT Controller address register is hex 18. The processor output port address for this register is hex 3B5 or hex 3D5.

<table>
  <tr>
    <th colspan="8">Line Compare Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Line Compare Target</th>
  </tr>
</table>

Bit 0–Bit 7    Line Compare—This register is the low-order 8 bits of the compare target. When the vertical
counter reaches this value, the internal start of the line counter is cleared. This allows an area of the screen to be immune to scrolling. Bit 8 of this register is in the overflow register hex 07.
Graphics Controller Registers

<table>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
  </tr>
  <tr>
    <td>Graphics 1 Position</td>
    <td>3CC</td>
    <td>-</td>
  </tr>
  <tr>
    <td>Graphics 2 Position</td>
    <td>3CA</td>
    <td>-</td>
  </tr>
  <tr>
    <td>Graphics 1 & 2 Address</td>
    <td>3CE</td>
    <td>-</td>
  </tr>
  <tr>
    <td>Set/Reset</td>
    <td>3CF</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Enable Set/Reset</td>
    <td>3CF</td>
    <td>01</td>
  </tr>
  <tr>
    <td>Color Compare</td>
    <td>3CF</td>
    <td>02</td>
  </tr>
  <tr>
    <td>Data Rotate</td>
    <td>3CF</td>
    <td>03</td>
  </tr>
  <tr>
    <td>Read Map Select</td>
    <td>3CF</td>
    <td>04</td>
  </tr>
  <tr>
    <td>Mode Register</td>
    <td>3CF</td>
    <td>05</td>
  </tr>
  <tr>
    <td>Miscellaneous</td>
    <td>3CF</td>
    <td>06</td>
  </tr>
  <tr>
    <td>Color Don't Care</td>
    <td>3CF</td>
    <td>07</td>
  </tr>
  <tr>
    <td>Bit Mask</td>
    <td>3CF</td>
    <td>08</td>
  </tr>
</table>

Graphics 1 Position Register

This is a write-only register. The processor output port address for this register is hex 3CC.

Graphics I Position Register Format

Bit 7 6 5 4 3 2 1 0
Position 0
Position 1
Not Used

Bit 0–Bit 1    Position—These 2 bits are binary encoded hierarchy bits for the graphics chips. The position register controls which 2 bits of the processor data bus each chip responds to. Graphics 1 must be programmed with a position register value of 0 for this card.
Graphics 2 Position Register

This is a write-only register. The processor output port address for this register is hex 3CA.

<table>
  <tr>
    <th colspan="8">Graphics II Position Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Position 0</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Position 1</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Not Used</td>
  </tr>
</table>

Bit 0–Bit 1 Position—These 2 bits are binary encoded hierarchy bits for the graphics chips. The position register controls which 2 bits of the processor data bus to which each chip responds. Graphics 2 must be programmed with a position register value of 1 for this card.

Graphics 1 and 2 Address Register

This is a write-only register and the processor output port address for this register is hex 3CE.

<table>
  <tr>
    <th colspan="8">Graphics 1 and 2 Address Register Formats</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Graphics Address</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Not Used</td>
  </tr>
</table>

Bit 0–Bit 3 Graphics 1 and 2 Address Bits—This output loads the address register in both graphics chips simultaneously. This register points to the data register of the graphics chips.
Set/Reset Register

This is a write-only register pointed to by the value in the Graphics 1 and 2 address register. This value must be hex 00 before writing can take place. The processor output port address for this register is hex 3CF.

<html>
  <table border="1">
    <tr>
      <th colspan="9">Set/Reset Register Format</th>
    </tr>
    <tr>
      <!-- Bit 7-0 columns omitted for brevity -->
      <td>Bit 7</td>
      <td>7</td>
      <td>Bit 6</td>
      <td>6</td>
      <td>Bit 5</td>
      <td>5</td>
      <td>Bit 4</td>
      <td>4</td>
      <td>Bit 3</td>
      <td>3</td>
      <td>Bit 2</td>
      <td>2</td>
      <td>Bit 1</td>
      <td>1</td>
      <td>Bit 0</td>
      <td>0</td>
      <td></td>
      <td></td>
      <td>Set/Reset Bit 0</td>
      <td>Set/Reset Bit 1</td>
      <td>Set/Reset Bit 2</td>
      <td>Set/Reset Bit 3</td>
      <td>Not Used</td>
    </tr>
  </table>
</html>

Bit 0–Bit 3   Set/Reset—These bits represent the value written to the respective memory planes when the processor does a memory write with write mode 0 selected and set/reset mode is enabled. Set/Reset can be enabled on a plane by plane basis with separate OUT commands to the Set/Reset register.

Enable Set/Reset Register

This is a write-only register and is pointed to by the value in the Graphics 1 and 2 address register. This value must be hex 01 before writing can take place. The processor output port for this register is hex 3CF.
Color Compare Register Format

<table>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
</table>

Color Compare 0
Color Compare 1
Color Compare 2
Color Compare 3
Not Used

Bit 0-Bit 3 Enable Set/Reset—These bits enable the set/reset function. The respective memory plane is written with the value of the Set/Reset register provided the write mode is 0. When write mode is 0 and Set/Reset is not enabled on a plane, that plane is written with the value of the processor data.

Color Compare Register

This is a write-only register pointed to by the value in the Graphics 1 and 2 address register. This value must be hex 02 before writing can take place. The processor output port address for this register is hex 3CF.

Enable Set/Reset Register Format

<table>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
</table>

Enable Set/Reset Bit 0
Enable Set/Reset Bit 1
Enable Set/Reset Bit 2
Enable Set/Reset Bit 3
Not Used

Bit 0-Bit 3 Color Compare—These bits represent a 4 bit color value to be compared. If the processor sets
read mode 1 on the graphics chips, and does a memory read, the data returned from the memory cycle will be a 1 in each bit position where the 4 bit planes equal the color compare register.

Data Rotate Register

This is a write-only register pointed to by the value in the Graphics 1 and 2 address register. This value must be hex 03 before writing can take place. The processor output port address for this register is hex 3CF.

<table>
  <tr>
    <th colspan="8">Data Rotate Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Rotate Count</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Rotate Count 1</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Rotate Count 2</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Function Select</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>Not Used</td>
  </tr>
</table>

Bit 0–Bit 2   Rotate Count—These bits represent a binary encoded value of the number of positions to rotate the processor data bus during processor memory writes. This operation is done when the write mode is 0. To write unrotated data the processor must select a count of 0.

Bit 3–Bit 4   Function Select—Data written to memory can operate logically with data already in the processor latches. The bit functions are defined in the following table.
Bits
4 3

0 0    Data unmodified.
0 1    Data AND'ed with latched data.
1 0    Data OR'ed with latched data.
1 1    Data XOR'ed with latched data.

Data may be any of the choices selected by the Write Mode Register except processor latches. If rotated data is selected, the rotate applies before the logical function.

Read Map Select Register

This is a write-only register pointed to by the value in the Graphics 1 and 2 address register. This value must be hex 04 before writing can take place. The processor output port address for this register is hex 3CF.

<table>
  <tr>
    <th colspan="8">Read Map Select Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Map Select 0</th>
    <th>Map Select 1</th>
    <th>Map Select 2</th>
    <th>Not Used</th>
  </tr>
</table>

Bit 0–Bit 2    Map Select—These bits represent a binary encoded value of the memory plane number from which the processor reads data. This register has no effect on the color compare read mode described elsewhere in this section.

Mode Register

This is a write-only register pointed to by the value in the Graphics 1 and 2 address register. This value must be hex 05
before writing can take place. The processor output port address for this register is 3CF.

![Mode Register Format diagram](page_156_164_1017_246.png)

Bit 0–Bit 1    Write Mode

Bits
1 0

0 0 Each memory plane is written with the processor data rotated by the number of counts in the rotate register, unless Set/Reset is enabled for the plane. Planes for which Set/Reset is enabled are written with 8 bits of the value contained in the Set/Reset register for that plane.
0 1 Each memory plane is written with the contents of the processor latches. These latches are loaded by a processor read operation.
1 0 Memory plane \( n \) (0 through 3) is filled with 8 bits of the value of data bit \( n \).
1 1 Not Valid

The logic function specified by the function select register also applies.

Bit 2    Test Condition—A logical 1 directs graphics controller outputs to be placed in high impedance state for testing.
Bit 3    Read Mode—When this bit is a logical 0, the processor reads data from the memory plane selected by the read map select register. When this bit is a logical 1, the processor reads the results of the comparison of the 4 memory planes and the color compare register.

Bit 4    Odd/Even—A logical 1 selects the odd/even addressing mode, which is useful for emulation of the Color Graphics Monitor Adapter compatible modes. Normally the value here follows the value of the Memory Mode Register bit 3 of the Sequencer.

Bit 5    Shift Register—A logical 1 directs the shift registers on each graphics chip to format the serial data stream with even numbered bits on the even numbered maps and odd numbered bits on the odd maps.

Miscellaneous Register

This is a write-only register pointed to by the value in the Graphics 1 and 2 address register. This value must be hex 06 before writing can take place. The processor output port for this register is hex 3CF.

<table>
  <tr>
    <th colspan="8">Miscellaneous Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td>Graphics Mode</td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td>Chain Odd Maps to Even</td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td>Memory Map 0</td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td>Memory Map 1</td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td>Not Used</td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
</table>
Bit 0    Graphics Mode—This bit controls alpha-mode addressing. A logical 1 selects graphics mode. When set to graphics mode, the character generator address latches are disabled.

Bit 1    Chain Odd Maps To Even Maps—When set to 1, this bit directs the processor address bit 0 to be replaced by a higher order bit and odd/even maps to be selected with odd/even values of the processor A0 bit, respectively.

Bit 2-Bit 3    Memory Map—These bits control the mapping of the regenerative buffer into the processor address space.

Bits
3   2
____
0   0   Hex A000 for 128K bytes.
0   1   Hex A000 for 64K bytes.
1   0   Hex B000 for 32K bytes
1   1   Hex B800 for 32K bytes.

If the display adapter is mapped at address hex A000 for 128K bytes, no other adapter can be installed in the system.

Color Don’t Care Register

This is a write-only register and is pointed to by the value in the Graphics 1 and 2 address register. This value must be hex 07 before writing can take place. The processor output port for this register is hex 3CF.
Color Don't Care Register Format

Bit 7 6 5 4 3 2 1 0
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |

Color Plane 0=Don't Care
Color Plane 1=Don't Care
Color Plane 2=Don't Care
Color Plane 3=Don't Care
Not Used

Bit 0    Color Don't Care—Color plane 0=don't care when reading color compare when this bit is set to 1.

Bit 1    Color Don't Care—Color plane 1=don't care when reading color compare when this bit is set to 1.

Bit 2    Color Don't Care—Color plane 2=don't care when reading color compare when this bit is set to 1.

Bit 3    Color Don't Care—Color plane 3=don't care when reading color compare when this bit is set to 1.

Bit Mask Register

This is a write-only register and is pointed to by the value in the Graphics 1 and 2 address register. This value must be hex 08 before writing can take place. The processor output port for this register is hex 3CF.

Bit Mask Register Format

Bit 7 6 5 4 3 2 1 0
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |
    |   |   |   |   |   |   |   |

0-Immune to change
1-Unimpeded Writes
Bit 0–Bit 7    Bit Mask—Any bit programmed to \( n \) causes the corresponding bit \( n \) in each bit plane to be immune to change provided that the location being written was the last location read by the processor. Bits programmed to a 1 allow unimpeded writes to the corresponding bits in the bit planes.

The bit mask applies to any data written by the processor (rotate, AND’ed, OR’ed, XOR’ed, DX, and S/R). To preserve bits using the bit mask, data must be latched internally by reading the location. When data is written to preserve the bits, the most current data in latches is written in those positions. The bit mask applies to all bit planes simultaneously.
Attribute Controller Registers

<table>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
  </tr>
  <tr>
    <td>Address Register</td>
    <td>3C0</td>
    <td>-</td>
  </tr>
  <tr>
    <td>Palette Registers</td>
    <td>3C0</td>
    <td>00-0F</td>
  </tr>
  <tr>
    <td>Mode Control Register</td>
    <td>3C0</td>
    <td>10</td>
  </tr>
  <tr>
    <td>Overscan Color Register</td>
    <td>3C0</td>
    <td>11</td>
  </tr>
  <tr>
    <td>Color Plane Enable Register</td>
    <td>3C0</td>
    <td>12</td>
  </tr>
  <tr>
    <td>Horizontal Pel Panning Register</td>
    <td>3C0</td>
    <td>13</td>
  </tr>
</table>

Attribute Address Register

This is a write-only register. The processor output port is hex 3C0.

<table>
  <tr>
    <th colspan="8">Palette Registers Hex 00 through Hex OF Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Blue Video</th>
    <th>Green Video</th>
    <th>Red Video</th>
    <th>Secondary Blue/Mono Video</th>
    <th>Secondary Green/Intensity</th>
    <th>Secondary Red Video</th>
    <th>Not Used</th>
  </tr>
</table>

Bit 0–Bit 4 Attribute Address Bits—The Address Register is a pointer register located at hex 3C0. This register is loaded with a binary value that points to the attribute data register where data is to be written. The Attribute Controller does not have an address bit input to control selection of the address and data registers. An internal address flip-flop controls selection of either the address or data registers. To initialize the flip-flop, an IOR instruction is issued to the Attribute Controller at address 3BA or 3DA. This clears the flip-flop, and selects the Address Register. After the Address Register has been loaded, the
next OUT instruction loads the data register.
The flip-flop toggles each time an OUT is issued to the Attribute Controller.

Bit 5
Palette Address Source—When loading the color palette registers, bit 5 must be cleared to 0. To enable the memory data to access the color palette, bit 5 must be set to 1.

Palette Register Hex 00 through Hex 0F

This is a write-only register. The processor output port is hex 3C0.

<table>
  <tr>
    <th colspan="8">Attribute Address Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Attribute Address</th>
    <th>Palette Address Source</th>
    <th>Not Used</th>
  </tr>
</table>

Bit 0-Bit 5
Palette—These 6-bit registers allow a dynamic mapping between the text attribute or graphic color input value and the display color in the CRT screen. A logical 1 selects the appropriate color. A logical 0 de-selects. The color palette register should be modified only during the vertical retrace interval to avoid glitches in the displayed image. Note that some color monitors do not have an intensity input and only a maximum of eight colors are be available. Monitors with four color inputs display sixteen colors, and monitors with six color inputs display 64 colors.
Mode Control Register

This is a write-only register pointed to by the value in the Attribute address register. This value must be hex 10 before writing can take place. The processor output port address for this register is hex 3C0.

<table>
  <tr>
    <th colspan="8">Mode Control Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th>Graphics/Alphanumeric Mode</th>
    <th>Display Type</th>
    <th>Enable Line Graphics Character Codes</th>
    <th>Select Background Intensity Or Enable Blink</th>
    <th>Not Used</th>
  </tr>
</table>

Bit 0    Graphics/Alphanumeric Mode—A logical 0 selects alphanumeric mode. A logical 1 selects graphics mode.

Bit 1    Monochrome Display/Color Display—A logical 0 selects color display attributes. A logical 1 selects IBM Monochrome Display attributes.

Bit 2    Enable Line Graphics Character Codes—When this bit is set to 0, the ninth dot will be the same as the background. A logical 1 enables the special line graphics character codes for the IBM Monochrome Display adapter. This bit when enabled forces the ninth dot of a line graphic character to be identical to the eighth dot of the character. The line graphics character codes for the Monochrome Display Adapter are Hex C0 through Hex DF.

For character fonts that do not utilize the line graphics character codes in the range of Hex C0
through Hex DF, bit 2 of this register should be a logical 0. Otherwise unwanted video information will be displayed on the CRT screen.

Bit 3    Enable Blink/Select Background Intensity—A logical 0 selects the background intensity of the attribute input. This mode was available on the Monochrome and Color Graphics adapters. A logical 1 enables the blink attribute in alphanumeric modes. This bit must also be set to 1 for blinking graphics modes.

Overscan Color Register

This is a write-only register pointed to by the value in the Attribute address register. This value must be hex 11 before writing can take place. The processor output port address for this register is hex 3C0.

<table>
  <tr>
    <th colspan="8">Overscan Color Register Format</th>
  </tr>
  <tr>
    <th>Bit</th>
    <th>7</th>
    <th>6</th>
    <th>5</th>
    <th>4</th>
    <th>3</th>
    <th>2</th>
    <th>1</th>
    <th>0</th>
    <th></th>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>→ Selects Blue Border Color</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>→ Selects Green Border Color</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>→ Selects Red Border Color</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>→ Selects Secondary Blue Border Color</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>→ Selects Intensified or Secondary Green</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>→ Selects Secondary Red Border Color</td>
  </tr>
  <tr>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td>→ Not Used</td>
  </tr>
</table>

Bit 0-Bit 5    Overscan Color—This 6-bit register determines the overscan (border) color displayed on the CRT screen. For monochrome display this register should be set to a value of 0. A logical 1 selects the appropriate color.
Color Plane Enable Register

This is a write-only register pointed to by the value in the Attribute address register. This value must be hex 12 before writing can take place. The processor output port address for this register is 3C0.

![Diagram showing the Color Plane Enable Register Format, with bit positions 7-0 labeled, and arrows pointing to Enable Color Plane, Video Status MUX, and Not Used](page_246_256_931_181.png)

Bit 0–Bit 3    Enable Color Plane—Writing a logical 1 in any of bits 0 through 3 enables the respective display memory color plane.

Bit 4–Bit 5    Video Status MUX—Selects two of the six color outputs to be available on the status port. The following table illustrates the combinations available and the color output wiring.

<table>
  <tr>
    <th>COLOR PLANE ENABLE REGISTER</th>
    <th>INPUT STATUS REGISTER ONE</th>
  </tr>
  <tr>
    <th>Bit 5</th>
    <th>Bit 4</th>
    <th>Bit 5</th>
    <th>Bit 4</th>
  </tr>
  <tr>
    <td>0</td>
    <td>0</td>
    <td>Red</td>
    <td>Blue</td>
  </tr>
  <tr>
    <td>0</td>
    <td>1</td>
    <td>Secondary Blue</td>
    <td>Green</td>
  </tr>
  <tr>
    <td>1</td>
    <td>0</td>
    <td>Secondary Red</td>
    <td>Secondary Green</td>
  </tr>
  <tr>
    <td>1</td>
    <td>1</td>
    <td>Not Used</td>
    <td>Not Used</td>
  </tr>
</table>

Horizontal Pel Panning Register

This is a write-only register pointed to by the value in the Attribute address register. This value must be hex 12 before writing can take place. The processor output port address for this register is hex 3C0.
Bit 0–Bit 3    Horizontal Pel Panning—This 4 bit register selects the number of picture elements (pels) to shift the video data horizontally to the left. Pel panning is available in both A/N and APA modes. In Monochrome A/N mode, the image can be shifted a maximum of 9 pels. In all other A/N and APA modes, the image can be shifted a maximum of 8 pels. The sequence for shifting the image is given below:

9 pels/character : 8, 0, 1, 2, 3, 4, 5, 6, 7
(Monochrome A/N mode only)

8 pels/character : 0, 1, 2, 3, 4, 5, 6, 7 (All other Modes)
Programming Considerations

Programming the Registers

Each of the LSI devices has an address register and a number of data registers. The address register serves as a pointer to the other registers on the LSI device. It is a write-only register that is loaded by the processor by executing an 'OUT' instruction to its I/O address with the index of the selected data register.

The data registers on each LSI device are accessed through a common I/O address. They are distinguished by the pointer (index) in the address register. To write to a data register, the address register is loaded with the index of the appropriate data register, then the selected data register is loaded by executing an 'OUT' instruction to the common I/O address.

The external registers that are not part of an LSI device and the Graphics I and II registers are not accessed through an address register; they are written to directly.

The following tables define the values that are loaded into the registers by BIOS to support the different modes of operation supported by this adapter.
<table>
  <tr>
    <th colspan="2">Register</th>
    <th colspan="13">Mode of Operation</th>
  </tr>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
    <th>0</th><th>1</th><th>2</th><th>3</th><th>4</th><th>5</th><th>6</th><th>7</th><th>D</th><th>E</th><th>F</th><th>10</th><th>F2</th><th>102</th><th>0*</th><th>1*</th><th>2*</th><th>3*</th>
  </tr>
  <tr>
    <td>Miscellaneous</td>
    <td>3C2</td>
    <td>-</td>
    <td>23</td><td>23</td><td>23</td><td>23</td><td>23</td><td>23</td><td>A6</td><td>23</td><td>23</td><td>A2</td><td>A7</td><td>A2</td><td>A7</td><td>A7</td><td>A7</td><td>A7</td><td>A7</td>
  </tr>
  <tr>
    <td>Feature Cntrl</td>
    <td>37A</td>
    <td>-</td>
    <td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td>
  </tr>
  <tr>
    <td>Input Stat 0</td>
    <td>3C2</td>
    <td>-</td>
    <td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td>
  </tr>
  <tr>
    <td>Input Stat 1</td>
    <td>322</td>
    <td>-</td>
    <td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td>
  </tr>
</table>

? = B in monochrome modes   ? = D in color modes

*Values for these modes when the IBM Enhanced Color Display is attached

:Values for these modes when greater than 64K Graphics Memory is installed

External Registers

<table>
  <tr>
    <th colspan="2">Register</th>
    <th colspan="13">Mode of Operation</th>
  </tr>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
    <th>0</th><th>1</th><th>2</th><th>3</th><th>4</th><th>5</th><th>6</th><th>7</th><th>D</th><th>E</th><th>F</th><th>10</th><th>F2</th><th>102</th><th>0*</th><th>1*</th><th>2*</th><th>3*</th>
  </tr>
  <tr>
    <td>Seq Address</td>
    <td>3C4</td>
    <td>-</td>
    <td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td>
  </tr>
  <tr>
    <td>Reset</td>
    <td>3C5</td>
    <td>00</td>
    <td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td>
  </tr>
  <tr>
    <td>Clock Mode</td>
    <td>3C5</td>
    <td>01</td>
    <td>OB</td><td>OB</td><td>01</td><td>OB</td><td>OB</td><td>01</td><td>00</td><td>OB</td><td>01</td><td>05</td><td>01</td><td>01</td><td>OB</td><td>OB</td><td>01</td><td>01</td><td>01</td>
  </tr>
  <tr>
    <td>Map Mask</td>
    <td>3C5</td>
    <td>02</td>
    <td>03</td><td>03</td><td>03</td><td>03</td><td>03</td><td>01</td><td>03</td><td>OF</td><td>OF</td><td>OF</td><td>OF</td><td>OF</td><td>03</td><td>03</td><td>03</td><td>03</td><td>03</td>
  </tr>
  <tr>
    <td>Char Gen Sel</td>
    <td>3C5</td>
    <td>03</td>
    <td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td>
  </tr>
  <tr>
    <td>Memory Mode</td>
    <td>3C5</td>
    <td>04</td>
    <td>03</td><td>03</td><td>03</td><td>02</td><td>02</td><td>05</td><td>03</td><td>06</td><td>06</td><td>00</td><td>00</td><td>06</td><td>06</td><td>03</td><td>03</td><td>03</td><td>03</td>
  </tr>
</table>

*Values for these modes when the IBM Enhanced Color Display is attached

:Values for these modes when greater than 64K Graphics Memory is installed

Sequencer Registers
<table>
  <tr>
    <th rowspan="2">Register</th>
    <th colspan="14">Mode of Operation</th>
  </tr>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
    <th>0</th>
    <th>1</th>
    <th>2</th>
    <th>3</th>
    <th>4</th>
    <th>5</th>
    <th>6</th>
    <th>7</th>
    <th>D</th>
    <th>E</th>
    <th>F</th>
    <th>10</th>
    <th>Fs</th>
    <th>10s</th>
    <th>0*</th>
    <th>1*</th>
    <th>2*</th>
    <th>3*</th>
  </tr>
  <tr>
    <td>Address Reg</td>
    <td>374</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
  </tr>
  <tr>
    <td>Horiz Total</td>
    <td>375</td>
    <td>00</td>
    <td>37</td>
    <td>37</td>
    <td>70</td>
    <td>70</td>
    <td>37</td>
    <td>37</td>
    <td>70</td>
    <td>60</td>
    <td>37</td>
    <td>70</td>
    <td>60</td>
    <td>58</td>
    <td>60</td>
    <td>5B</td>
    <td>2D</td>
    <td>2D</td>
    <td>5B</td>
    <td>5B</td>
  </tr>
  <tr>
    <td>Hz Disp End</td>
    <td>375</td>
    <td>01</td>
    <td>27</td>
    <td>27</td>
    <td>4F</td>
    <td>4F</td>
    <td>27</td>
    <td>27</td>
    <td>4F</td>
    <td>4F</td>
    <td>27</td>
    <td>4F</td>
    <td>4F</td>
    <td>4F</td>
    <td>4F</td>
    <td>27</td>
    <td>27</td>
    <td>4F</td>
    <td>4F</td>
    <td>4F</td>
    <td>4F</td>
  </tr>
  <tr>
    <td>Strt Hz Blk</td>
    <td>375</td>
    <td>02</td>
    <td>2D</td>
    <td>2D</td>
    <td>5C</td>
    <td>5C</td>
    <td>2D</td>
    <td>2D</td>
    <td>59</td>
    <td>56</td>
    <td>2D</td>
    <td>56</td>
    <td>56</td>
    <td>53</td>
    <td>56</td>
    <td>53</td>
    <td>2B</td>
    <td>2B</td>
    <td>53</td>
    <td>53</td>
    <td>53</td>
  </tr>
  <tr>
    <td>End Hz Blk</td>
    <td>375</td>
    <td>03</td>
    <td>37</td>
    <td>37</td>
    <td>2F</td>
    <td>2F</td>
    <td>37</td>
    <td>37</td>
    <td>2D</td>
    <td>3A</td>
    <td>37</td>
    <td>2D</td>
    <td>1A</td>
    <td>17</td>
    <td>3A</td>
    <td>37</td>
    <td>2D</td>
    <td>2D</td>
    <td>37</td>
    <td>37</td>
    <td>37</td>
  </tr>
  <tr>
    <td>Strt Hz Retr</td>
    <td>375</td>
    <td>04</td>
    <td>31</td>
    <td>31</td>
    <td>5F</td>
    <td>5F</td>
    <td>30</td>
    <td>30</td>
    <td>5E</td>
    <td>51</td>
    <td>30</td>
    <td>5E</td>
    <td>50</td>
    <td>50</td>
    <td>52</td>
    <td>28</td>
    <td>28</td>
    <td>51</td>
    <td>51</td>
    <td>51</td>
    <td>51</td>
  </tr>
  <tr>
    <td>End Hz Retr</td>
    <td>375</td>
    <td>05</td>
    <td>15</td>
    <td>15</td>
    <td>07</td>
    <td>07</td>
    <td>14</td>
    <td>14</td>
    <td>06</td>
    <td>60</td>
    <td>14</td>
    <td>06</td>
    <td>E0</td>
    <td>BA</td>
    <td>60</td>
    <td>00</td>
    <td>6D</td>
    <td>6D</td>
    <td>5B</td>
    <td>5B</td>
    <td>5B</td>
  </tr>
  <tr>
    <td>Vert Total</td>
    <td>375</td>
    <td>06</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
    <td>70</td>
    <td>04</td>
    <td>04</td>
    <td>70</td>
    <td>6C</td>
    <td>70</td>
    <td>6C</td>
    <td>6C</td>
    <td>6C</td>
    <td>6C</td>
    <td>6C</td>
    <td>6C</td>
  </tr>
  <tr>
    <td>Overflow</td>
    <td>375</td>
    <td>07</td>
    <td>11</td>
    <td>11</td>
    <td>11</td>
    <td>11</td>
    <td>11</td>
    <td>11</td>
    <td>11</td>
    <td>11</td>
    <td>11</td>
    <td>11</td>
    <td>1F</td>
    <td>1F</td>
    <td>1F</td>
    <td>1F</td>
    <td>1F</td>
    <td>1F</td>
    <td>1F</td>
    <td>1F</td>
    <td>1F</td>
  </tr>
  <tr>
    <td>Preset Row SC</td>
    <td>375</td>
    <td>08</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Max Scan Line</td>
    <td>375</td>
    <td>09</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>0D</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>0D</td>
    <td>0D</td>
    <td>0D</td>
    <td>0D</td>
    <td>0D</td>
  </tr>
  <tr>
    <td>Cursor Start</td>
    <td>375</td>
    <td>0A</td>
    <td>06</td>
    <td>06</td>
    <td>06</td>
    <td>06</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>OB</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>0B</td>
    <td>0B</td>
    <td>0B</td>
    <td>0B</td>
    <td>0B</td>
    <td>0B</td>
  </tr>
  <tr>
    <td>Cursor End</td>
    <td>375</td>
    <td>0B</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>OC</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>0C</td>
    <td>0C</td>
    <td>0C</td>
    <td>0C</td>
    <td>0C</td>
    <td>0C</td>
  </tr>
  <tr>
    <td>Strt Addr Hi</td>
    <td>375</td>
    <td>0C</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
  </tr>
  <tr>
    <td>Strt Addr Lo</td>
    <td>375</td>
    <td>0D</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
  </tr>
</table>

? = B in monochrome modes   ? = D in color modes

*Values for these modes when the IBM Enhanced Color Display is attached

:Values for these modes when greater than 64K Graphics Memory is installed

CRT Controller Registers (1 of 2)
<table>
  <tr>
    <th rowspan="2">Register</th>
    <th colspan="14">Mode of Operation</th>
  </tr>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
    <th>0</th>
    <th>1</th>
    <th>2</th>
    <th>3</th>
    <th>4</th>
    <th>5</th>
    <th>6</th>
    <th>7</th>
    <th>D</th>
    <th>E</th>
    <th>F</th>
    <th>10</th>
    <th>F2</th>
    <th>102</th>
    <th>0*</th>
    <th>1*</th>
    <th>2*</th>
    <th>3*</th>
  </tr>
  <tr>
    <td>Cursor LC Hi</td>
    <td>375</td>
    <td>OE</td>
    <td></td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td>
  </tr>
  <tr>
    <td>Cursor LC Low</td>
    <td>375</td>
    <td>OF</td>
    <td></td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td>
  </tr>
  <tr>
    <td>Vrt Retr Strt</td>
    <td>375</td>
    <td>10</td>
    <td>E1</td><td>E1</td><td>E1</td><td>E1</td><td>E1</td><td>E0</td><td>SE</td><td>E1</td><td>E0</td><td>SE</td><td>SE</td><td>SE</td><td>SE</td><td>SE</td><td>SE</td><td>SE</td><td>SE</td><td>SE</td>
  </tr>
  <tr>
    <td>Light Pen Hi</td>
    <td>375</td>
    <td>10</td>
    <td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td>
  </tr>
  <tr>
    <td>Vert Retr End</td>
    <td>375</td>
    <td>11</td>
    <td>24</td><td>24</td><td>24</td><td>24</td><td>24</td><td>23</td><td>2E</td><td>24</td><td>23</td><td>2E</td><td>2B</td><td>2E</td><td>2B</td><td>2B</td><td>2B</td><td>2B</td><td>2B</td><td>2B</td>
  </tr>
  <tr>
    <td>Light Pen Low</td>
    <td>375</td>
    <td>11</td>
    <td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td><td>-</td>
  </tr>
  <tr>
    <td>Vrt Disp End</td>
    <td>375</td>
    <td>12</td>
    <td>C7</td><td>C7</td><td>C7</td><td>C7</td><td>C7</td><td>C7</td><td>5D</td><td>C7</td><td>C7</td><td>5D</td><td>5D</td><td>5D</td><td>5D</td><td>5D</td><td>5D</td><td>5D</td><td>5D</td><td>5D</td>
  </tr>
  <tr>
    <td>Offset</td>
    <td>375</td>
    <td>13</td>
    <td>14</td><td>14</td><td>28</td><td>28</td><td>14</td><td>14</td><td>28</td><td>14</td><td>28</td><td>14</td><td>28</td><td>28</td><td>14</td><td>14</td><td>28</td><td>28</td><td>14</td><td>28</td><td>28</td>
  </tr>
  <tr>
    <td>Underline Loc</td>
    <td>375</td>
    <td>14</td>
    <td>08</td><td>08</td><td>08</td><td>08</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>00</td><td>0F</td><td>0D</td><td>0F</td><td>0F</td><td>0F</td><td>0F</td><td>0F</td><td>0F</td><td>0F</td>
  </tr>
  <tr>
    <td>Strt Vert Blk</td>
    <td>375</td>
    <td>15</td>
    <td>E0</td><td>E0</td><td>E0</td><td>E0</td><td>E0</td><td>DF</td><td>5E</td><td>E0</td><td>DF</td><td>5E</td><td>5F</td><td>5E</td><td>5F</td><td>5E</td><td>5E</td><td>5E</td><td>5E</td><td>5E</td><td>5E</td>
  </tr>
  <tr>
    <td>End Vert Blk</td>
    <td>375</td>
    <td>16</td>
    <td>F0</td><td>F0</td><td>F0</td><td>F0</td><td>F0</td><td>EF</td><td>6E</td><td>F0</td><td>EF</td><td>6E</td><td>0A</td><td>6E</td><td>0A</td><td>0A</td><td>0A</td><td>0A</td><td>0A</td><td>0A</td><td>0A</td>
  </tr>
  <tr>
    <td>Mode Control</td>
    <td>375</td>
    <td>17</td>
    <td>A3</td><td>A3</td><td>A3</td><td>A3</td><td>A2</td><td>A2</td><td>C2</td><td>A3</td><td>E3</td><td>8B</td><td>8B</td><td>E3</td><td>E3</td><td>A3</td><td>A3</td><td>A3</td><td>A3</td><td>A3</td><td>A3</td>
  </tr>
  <tr>
    <td>Line Compare</td>
    <td>375</td>
    <td>18</td>
    <td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td><td>FF</td>
  </tr>
</table>

? = B in monochrome modes   ? = D in color modes

*Values for these modes when the IBM Enhanced Color Display is attached

:Values for these modes when greater than 64K Graphics Memory is installed

CRT Controller Registers (2 of 2)
<table>
  <tr>
    <th rowspan="2">Register</th>
    <th colspan="15">Mode of Operation</th>
  </tr>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
    <th>0</th>
    <th>1</th>
    <th>2</th>
    <th>3</th>
    <th>4</th>
    <th>5</th>
    <th>6</th>
    <th>7</th>
    <th>D</th>
    <th>E</th>
    <th>F</th>
    <th>10</th>
    <th>F8</th>
    <th>10s</th>
    <th>0*</th>
    <th>1*</th>
    <th>2*</th>
    <th>3*</th>
  </tr>
  <tr>
    <td>Grphx I Pos</td>
    <td>3CC</td>
    <td>-</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Grphx II Pos</td>
    <td>3CA</td>
    <td>-</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
  </tr>
  <tr>
    <td>Grphx I II AD</td>
    <td>3CE</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
  </tr>
  <tr>
    <td>Set Reset</td>
    <td>3CF</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Enable S/R</td>
    <td>3CF</td>
    <td>01</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Color Compare</td>
    <td>3CF</td>
    <td>02</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Data Rotate</td>
    <td>3CF</td>
    <td>03</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Read Map Sel</td>
    <td>3CF</td>
    <td>04</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Mode Register</td>
    <td>3CF</td>
    <td>05</td>
    <td>10</td>
    <td>10</td>
    <td>10</td>
    <td>10</td>
    <td>30</td>
    <td>30</td>
    <td>00</td>
    <td>10</td>
    <td>00</td>
    <td>10</td>
    <td>10</td>
    <td>00</td>
    <td>00</td>
    <td>10</td>
    <td>10</td>
    <td>10</td>
    <td>10</td>
    <td>10</td>
  </tr>
  <tr>
    <td>Miscellaneous</td>
    <td>3CF</td>
    <td>06</td>
    <td>0E</td>
    <td>0E</td>
    <td>0E</td>
    <td>0E</td>
    <td>OF</td>
    <td>OF</td>
    <td>OD</td>
    <td>0A</td>
    <td>05</td>
    <td>05</td>
    <td>07</td>
    <td>07</td>
    <td>05</td>
    <td>05</td>
    <td>OE</td>
    <td>OE</td>
    <td>OE</td>
    <td>OE</td>
    <td>OE</td>
  </tr>
  <tr>
    <td>Color No Care</td>
    <td>3CF</td>
    <td>07</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>0F</td>
    <td>0F</td>
    <td>0F</td>
    <td>0F</td>
    <td>0F</td>
    <td>0F</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Bit Mask</td>
    <td>3CF</td>
    <td>08</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
    <td>FF</td>
  </tr>
</table>

*Values for these modes when the IBM Enhanced Color Display is attached

:Values for these modes when greater than 64 K Graphics Memory is installed

Graphics SI Registers
<table>
  <tr>
    <th rowspan="2">Register</th>
    <th colspan="14">Mode of Operation</th>
  </tr>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
    <th>0</th>
    <th>1</th>
    <th>2</th>
    <th>3</th>
    <th>4</th>
    <th>5</th>
    <th>6</th>
    <th>7</th>
    <th>D</th>
    <th>E</th>
    <th>F</th>
    <th>10</th>
    <th>F8</th>
    <th>10S</th>
    <th>0*</th>
    <th>1*</th>
    <th>2*</th>
    <th>3*</th>
  </tr>
  <tr>
    <td>Address</td>
    <td>3?A</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
    <td>-</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>13</td>
    <td>13</td>
    <td>17</td>
    <td>08</td>
    <td>01</td>
    <td>01</td>
    <td>08</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>02</td>
    <td>02</td>
    <td>02</td>
    <td>02</td>
    <td>15</td>
    <td>15</td>
    <td>17</td>
    <td>08</td>
    <td>02</td>
    <td>02</td>
    <td>00</td>
    <td>00</td>
    <td>02</td>
    <td>02</td>
    <td>02</td>
    <td>02</td>
    <td>02</td>
    <td>02</td>
    <td>02</td>
    <td>02</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>03</td>
    <td>03</td>
    <td>03</td>
    <td>03</td>
    <td>17</td>
    <td>17</td>
    <td>17</td>
    <td>08</td>
    <td>03</td>
    <td>03</td>
    <td>00</td>
    <td>00</td>
    <td>03</td>
    <td>03</td>
    <td>03</td>
    <td>03</td>
    <td>03</td>
    <td>03</td>
    <td>03</td>
    <td>03</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
    <td>02</td>
    <td>02</td>
    <td>17</td>
    <td>08</td>
    <td>04</td>
    <td>04</td>
    <td>18</td>
    <td>04</td>
    <td>18</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
    <td>04</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>05</td>
    <td>05</td>
    <td>05</td>
    <td>05</td>
    <td>04</td>
    <td>04</td>
    <td>17</td>
    <td>08</td>
    <td>05</td>
    <td>05</td>
    <td>18</td>
    <td>07</td>
    <td>18</td>
    <td>05</td>
    <td>05</td>
    <td>05</td>
    <td>05</td>
    <td>05</td>
    <td>05</td>
    <td>05</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>06</td>
    <td>06</td>
    <td>06</td>
    <td>06</td>
    <td>06</td>
    <td>06</td>
    <td>17</td>
    <td>08</td>
    <td>06</td>
    <td>06</td>
    <td>00</td>
    <td>00</td>
    <td>06</td>
    <td>14</td>
    <td>14</td>
    <td>14</td>
    <td>14</td>
    <td>14</td>
    <td>14</td>
    <td>14</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
    <td>17</td>
    <td>08</td>
    <td>07</td>
    <td>07</td>
    <td>00</td>
    <td>00</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
    <td>07</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>08</td>
    <td>10</td>
    <td>10</td>
    <td>10</td>
    <td>10</td>
    <td>10</td>
    <td>17</td>
    <td>10</td>
    <td>10</td>
    <td>10</td>
    <td>00</td>
    <td>00</td>
    <td>38</td>
    <td>38</td>
    <td>38</td>
    <td>38</td>
    <td>38</td>
    <td>38</td>
    <td>38</td>
    <td>38</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>09</td>
    <td>11</td>
    <td>11</td>
    <td>11</td>
    <td>11</td>
    <td>11</td>
    <td>17</td>
    <td>18</td>
    <td>11</td>
    <td>11</td>
    <td>08</td>
    <td>01</td>
    <td>08</td>
    <td>39</td>
    <td>39</td>
    <td>39</td>
    <td>39</td>
    <td>39</td>
    <td>39</td>
    <td>39</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>0A</td>
    <td>12</td>
    <td>12</td>
    <td>12</td>
    <td>12</td>
    <td>12</td>
    <td>17</td>
    <td>18</td>
    <td>12</td>
    <td>12</td>
    <td>00</td>
    <td>00</td>
    <td>3A</td>
    <td>3A</td>
    <td>3A</td>
    <td>3A</td>
    <td>3A</td>
    <td>3A</td>
    <td>3A</td>
    <td>3A</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3C0</td>
    <td>0B</td>
    <td>13</td>
    <td>13</td>
    <td>13</td>
    <td>13</td>
    <td>13</td>
    <td>13</td>
    <td>17</td>
    <td>18</td>
    <td>13</td>
    <td>13</td>
    <td>00</td>
    <td>00</td>
    <td>3B</td>
    <td>3B</td>
    <td>3B</td>
    <td>3B</td>
    <td>3B</td>
    <td>3B</td>
    <td>3B</td>
  </tr>
</table>

? = B in monochrome modes   ? = D in color modes

*Values for these modes when the IBM Enhanced Color Display is attached

#Values for these modes when greater than 64K Graphics Memory is installed

Attribute Registers (1 of 2)
<table>
  <tr>
    <th rowspan="2">Register</th>
    <th colspan="14">Mode of Operation</th>
  </tr>
  <tr>
    <th>Name</th>
    <th>Port</th>
    <th>Index</th>
    <th>0</th>
    <th>1</th>
    <th>2</th>
    <th>3</th>
    <th>4</th>
    <th>5</th>
    <th>6</th>
    <th>7</th>
    <th>D</th>
    <th>E</th>
    <th>F</th>
    <th>10</th>
    <th>F2</th>
    <th>10*</th>
    <th>0*</th>
    <th>1*</th>
    <th>2*</th>
    <th>3*</th>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3CO</td>
    <td>OC</td>
    <td>14</td>
    <td>14</td>
    <td>14</td>
    <td>14</td>
    <td>14</td>
    <td>14</td>
    <td>17</td>
    <td>18</td>
    <td>14</td>
    <td>14</td>
    <td>00</td>
    <td>04</td>
    <td>00</td>
    <td>3C</td>
    <td>3C</td>
    <td>3C</td>
    <td>3C</td>
    <td>3C</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3CO</td>
    <td>OD</td>
    <td>15</td>
    <td>15</td>
    <td>15</td>
    <td>15</td>
    <td>15</td>
    <td>15</td>
    <td>17</td>
    <td>18</td>
    <td>15</td>
    <td>15</td>
    <td>18</td>
    <td>07</td>
    <td>18</td>
    <td>3D</td>
    <td>3D</td>
    <td>3D</td>
    <td>3D</td>
    <td>3D</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3CO</td>
    <td>OE</td>
    <td>16</td>
    <td>16</td>
    <td>16</td>
    <td>16</td>
    <td>16</td>
    <td>17</td>
    <td>18</td>
    <td>16</td>
    <td>16</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>3E</td>
    <td>3E</td>
    <td>3E</td>
    <td>3E</td>
    <td>3E</td>
    <td>3E</td>
  </tr>
  <tr>
    <td>Palette</td>
    <td>3CO</td>
    <td>OF</td>
    <td>17</td>
    <td>17</td>
    <td>17</td>
    <td>17</td>
    <td>17</td>
    <td>17</td>
    <td>18</td>
    <td>17</td>
    <td>17</td>
    <td>00</td>
    <td>00</td>
    <td>3F</td>
    <td>3F</td>
    <td>3F</td>
    <td>3F</td>
    <td>3F</td>
    <td>3F</td>
    <td>3F</td>
  </tr>
  <tr>
    <td>Mode Control</td>
    <td>3CO</td>
    <td>10</td>
    <td>08</td>
    <td>08</td>
    <td>08</td>
    <td>08</td>
    <td>01</td>
    <td>01</td>
    <td>01</td>
    <td>0E</td>
    <td>01</td>
    <td>01</td>
    <td>OB</td>
    <td>OB</td>
    <td>OB</td>
    <td>01</td>
    <td>08</td>
    <td>08</td>
    <td>08</td>
    <td>08</td>
    <td>08</td>
  </tr>
  <tr>
    <td>Overscan</td>
    <td>3CO</td>
    <td>11</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
  </tr>
  <tr>
    <td>Color Plane</td>
    <td>3CO</td>
    <td>12</td>
    <td>0F</td>
    <td>0F</td>
    <td>0F</td>
    <td>0F</td>
    <td>03</td>
    <td>03</td>
    <td>01</td>
    <td>0F</td>
    <td>0F</td>
    <td>05</td>
    <td>05</td>
    <td>05</td>
    <td>0F</td>
    <td>0F</td>
    <td>0F</td>
    <td>0F</td>
    <td>0F</td>
    <td>0F</td>
    <td>0F</td>
  </tr>
  <tr>
    <td>Hz Panning</td>
    <td>3CO</td>
    <td>13</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
    <td>00</td>
  </tr>
</table>

*Values for these modes when the IBM Enhanced Color Display is attached

:Values for these modes when greater than 64K Graphics Memory is installed

Attribute Registers (2 of 2)
RAM Loadable Character Generator

The character generator on the adapter is RAM loadable and can support characters up to 32 scan lines high. Two character generators are stored within the BIOS and one is automatically loaded into the RAM by the BIOS when an alphanumeric mode is selected. The Character Map Select Register can be programmed to define the function of bit 3 of the attribute byte to be a character generator switch. This allows the user to select between any two character sets residing in bit plane 2. This effectively gives the user access to 512 characters instead of 256. character tables may be loaded off line. The adapter must have 128K bytes of storage to support this function. Up to four tables can be loaded can be loaded with 256K of graphics memory installed.

The structure of the character tables is described in the following figure. The character generator is in bit plane 2 and must be protected using the map mask function.

![Diagram showing Bit Plane 2 with four character generators labeled Character Generator 0, Character Generator 1, Character Generator 2, Character Generator 3](page_495_728_388_388.png)

The following figure illustrates the structure of each character pattern. If the CRT controller is programmed to generate \( n \) row
scans, then \( n \) bytes must be filled in for each character in the character generator. The example assumes eight row scans per character.

![Byte image diagram showing a character code layout with corresponding data values](page_184_328_670_495.png)

CC = Value of the character code. For example, 41H in the case of an ASCII "A".

Creating a 512 Character Set

This section describes how to create a 512 character set on the IBM Color Display. Note that only 256 characters can be printed on the printer. This is a special application which the Enhanced Graphics Adapter will support. The 9 by 14 characters will be displayed when attribute bit 3 is a logical 0, and the IBM Color/Graphics Monitor Adapter 8 by 8 characters will be displayed when the attribute bit 3 is a logical 1. This example is for demonstrative purposes only. The assembly language routine for creating 512 characters is given below. Debug 2.0 was used for this example. The starting assembly address is 100 and the character string is stored in location 200. This function requires 128K or more of graphics memory.
a100
mov ax,1102 ;load 8x8 character font in character
mov bl,02 ;generator number 2
int 10

mov ax,1103 ;select 512 character operation
mov bl,08 ;if attribute bit 3=1 use 8x8 font
int 10 ;if attribute bit 3=0 use 9x14 font

mov ax,1000 ;set color plane enable to 7H to disable
mov bx,0712 ;attribute bit 3 in the color palette
int 10 ;lookup table

mov ax,1301
mov bx,000F ;write char. string with attribute bit 3=1
mov cx,003A ;cx = character string length
mov dx,1600 ;write character on line 22 of display
mov bp,0200 ;pointer to character string location
push cs
pop es
int 10

mov ax ,1301
mov bx,0007 ;write char. string with attribute bit 3=0
mov cx,003A ;cx = character string length
mov dx,1700 ;write character on line 23 of display
mov bp,0200 ;pointer to character string location
push cs
pop es
int 10
int 3

a200 db "This character string is used to show 512 characters"

Creating an 80 by 43 Alphanumeric Mode

The following examples show how to create 80 column by 43 row, both alphanumeric and graphics, images on the IBM Monochrome Display. The BIOS Interface supports an 80 column by n row display by using the character generator load routine call. The print screen routine must be revectored to
handle the additional character rows on the screen. The assembly language required for both an alphanumeric and a graphics screen is shown below.

mov al,7           ;Monochrome alphanumeric mode
int 10             ;video interrupt call
mov ax,1112        ;character generator BIOS routine
mov bl,0           ;load 8 by 8 double dot character font
int 10             ;video interrupt call
mov ax,1200        ;alternate screen routine
move bl,20         ;select alternate print screen routine
int 10             ;video interrupt call
int 3

mov ax,f           ;Monochrome graphic mode
int 10             ;video interrupt call
mov ax,1123        ;character generator BIOS routine
mov bl,0           ;load 8 by 8 double dot character font
mov dl,2B          ;43 character rows
int 10             ;video interrupt call
mov ax,1200        ;alternate screen routine
mov bl,20         ;alternate print screen routine
int 10             ;video interrupt call
int 3

Vertical Interrupt Feature

The Enhanced Graphics Adapter can be programmed to create an interrupt each time the vertical display refresh time has ended. An interrupt handler routine must be written by the application to take advantage of this feature. The CRT Vertical interrupt is on IRQ2. The CPU can poll the Enhanced Graphics Adapter Input Status Register 0 (bit 7) to determine whether the CRTC caused the interrupt to occur.

The Vertical Retrace End Register (11H) in the CRT controller contains two bits which are used to control the interrupt circuitry. The remaining bits must be output as per the value in the mode table.
Bit 5    Enable Vertical Interrupt—A logical 0 will enable vertical interrupt.

Bit 4    Clear Vertical Interrupt—A logical 0 will clear a vertical interrupt.

The sequence of events which occur in an interrupt handler are outlined below.

1. Clear IRQ latch and enable driver
2. Enable IRQ latch
3. Wait for vertical interrupt
4. Poll Interrupt Status Register 0 to determine if CRTC has caused the interrupt
5. If CRTC interrupt, then clear IRQ latch; if not, then branch to next interrupt handler.
6. Enable IRQ latch
7. Update Enhanced Graphics Adapter during vertical blanking interval
8. Wait for next vertical interrupt

Creating a Split Screen

The Enhanced Graphics Adapter hardware supports an alphanumeric mode dual screen display. The top portion of the screen is designated as screen A, and the bottom portion of the screen is designated as screen B as per the following figure.

Screen A
Screen B

Dual Screen Definition

The following figure shows the screen mapping for a system containing a 32K byte alphanumeric storage buffer. Note that the Enhanced Graphics Adapter has a 32K byte storage buffer in alphanumeric mode. Information displayed on screen A is
defined by the start address high and low registers (0CH and 0DH) of the CRTC. Information displayed on screen B always begins at address 0000H.

Screen Mapping Within the Display Buffer Address Space

The Line Compare Register (18H) of the CRT Controller is utilized to perform the split screen function. The CRTC has an internal horizontal scan counter, and logic which compares the horizontal scan counter value to the Line Compare Register value and clears the memory address generator when a compare occurs. The linear address generator then sequentially addresses the display buffer starting at location zero, and each subsequent row address is determined by the 16 bit addition of the start of line latch and the offset register.

Screen B can be smoothly scrolled onto the CRT screen by updating the Line compare in synchronization with the vertical retrace signal. The information on screen B is immune from scrolling operations which utilize the Start Address High and Low registers to scroll through the Screen A address map.

Compatibility Issues

The CRT Controller on the IBM Enhanced Graphics Adapter is a custom design, and is different than the 6845 controller used on the IBM Monochrome Monitor Adapter and the IBM Color/Graphics Monitor Adapter. It should be noted that several CRTC register addresses differ between the adapters. The following figure illustrates the registers which do not map directly across the two controllers.
<table>
  <tr>
    <th>Register</th>
    <th>6485 Function</th>
    <th>EGA CRTC Function</th>
  </tr>
  <tr>
    <td>02H</td>
    <td>Start Horiz. Retrace</td>
    <td>Start Horiz. Blanking</td>
  </tr>
  <tr>
    <td>03H</td>
    <td>End Horiz. Retrace</td>
    <td>End Horiz. Blanking</td>
  </tr>
  <tr>
    <td>04H</td>
    <td>Vertical Total</td>
    <td>Start Horiz. Retrace</td>
  </tr>
  <tr>
    <td>05H</td>
    <td>Vertical Total Adjust</td>
    <td>End Horiz. Retrace</td>
  </tr>
  <tr>
    <td>06H</td>
    <td>Vertical Displayed</td>
    <td>Vertical Total</td>
  </tr>
  <tr>
    <td>07H</td>
    <td>Vertical Sync Position</td>
    <td>Overflow</td>
  </tr>
  <tr>
    <td>08H</td>
    <td>Interlace Mode and Skew</td>
    <td>Preset Row Scan</td>
  </tr>
</table>

Existing applications which utilize the BIOS interface will generally be compatible with the Enhanced Graphics Adapter.

Horizontal screen centering was required on the IBM Color/Graphics Monitor Adapter in order to center the screen when generating composite video. This was done through the Horizontal Sync Position Register. Since the Enhanced Graphics Adapter does not support a composite video monitor, programs which do screen centering may cause loss of the screen image if centering is attempted.

The Enhanced Graphics Adapter offers a wider variety of displayable monochrome character attributes than the IBM Monochrome Display Adapter. Some attribute values may display differently between the two Adapters. The values listed in the table below, in any combinations with the blink and intensity attributes, will display identically.

<table>
  <tr>
    <th>Background<br>R G B</th>
    <th>Foreground<br>R G B</th>
    <th>Function</th>
  </tr>
  <tr>
    <td>0 0 0</td>
    <td>0 0 0</td>
    <td>Non-Display</td>
  </tr>
  <tr>
    <td>0 0 0</td>
    <td>0 0 1</td>
    <td>Underline</td>
  </tr>
  <tr>
    <td>0 0 0</td>
    <td>1 1 1</td>
    <td>White Character/Black Background</td>
  </tr>
  <tr>
    <td>1 1 1</td>
    <td>0 0 0</td>
    <td>Reverse Video</td>
  </tr>
</table>

Software which explicitly addresses 3D8 (Mode Select Register) or 3D9 (Color Select Register) on the Color Graphics Monitor Adapter may produce different results on the Enhanced Graphics Adapter. For example, blinking which is disabled by writing to 3D8 on the Color Graphics Adapter will not be disabled on the Enhanced Graphics Adapter.
Interface

Feature Connector

The following is a description of the Enhanced Graphics Adapter feature connector. Note that signals coming from the Enhanced Graphics Adapter are labeled "inputs" and the signals coming to the Enhanced Graphics Adapter through the feature connector are labeled "outputs".

<table>
  <tr>
    <th>Signal</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>J2</td>
    <td>This pin is connected to auxiliary jack 2 on the rear panel of the adapter.</td>
  </tr>
  <tr>
    <td>R'OUT</td>
    <td>Secondary red output</td>
  </tr>
  <tr>
    <td>ATRS/L</td>
    <td>Attribute shift load. This signal controls the serialization of the video information. The shift register parallel loads at the dot clock leading edge when this signal is low.</td>
  </tr>
  <tr>
    <td>G OUT</td>
    <td>Primary green output</td>
  </tr>
  <tr>
    <td>R'</td>
    <td>Secondary red input</td>
  </tr>
  <tr>
    <td>R</td>
    <td>Primary red input</td>
  </tr>
  <tr>
    <td>FC1</td>
    <td>This signal is input from bit 1 (Feature Control Bit 1) of the Feature Control Register.</td>
  </tr>
  <tr>
    <td>FC0</td>
    <td>This signal is input from bit 0 (Feature Control Bit 0) of the Feature control Register.</td>
  </tr>
  <tr>
    <td>FEAT 0</td>
    <td>This signal is output to bit 5 (Feature Code 0) of Input Status Register 0.</td>
  </tr>
  <tr>
    <td>B'/V</td>
    <td>Secondary blue input/Monochrome video</td>
  </tr>
  <tr>
    <td>VIN</td>
    <td>Vertical retrace input</td>
  </tr>
</table>
Internal    This signal is output to bit 4 (Disable Internal Video Drivers) of the Miscellaneous Output Register.

V OUT       Vertical retrace output

J1          This pin is connected to auxiliary jack 1 on the rear panel of the adapter.

G'OUT       Secondary green output

B'OUT       Secondary blue output

B OUT       Blue output

G           Green input

B           Blue input

R OUT       Red output

BLANK       This is a composite horizontal and vertical blanking signal from the CRTC.

FEAT 1      This signal is output to bit 6 (Feature Code 1) of Input Status Register 0.

G'/I        Secondary green/Intensity input

HIN         Horizontal retrace input from the CRTC

14MHZ       14 MHz signal from the system board

EXT OSC     External dot clock output

HOUT        Horizontal retrace output
The following figure shows the layout and pin numbering of the feature connector.

<table>
  <tr>
    <th>Signal Name</th>
    <th>Signal Name</th>
  </tr>
  <tr>
    <td>Gnd</td>
    <td>-12V</td>
  </tr>
  <tr>
    <td>+12V</td>
    <td>J1</td>
  </tr>
  <tr>
    <td>J2</td>
    <td>G'OUT</td>
  </tr>
  <tr>
    <td>R'OUT</td>
    <td>B'OUT</td>
  </tr>
  <tr>
    <td>ATRS/L</td>
    <td>B OUT</td>
  </tr>
  <tr>
    <td>G OUT</td>
    <td>G</td>
  </tr>
  <tr>
    <td>R'</td>
    <td>B</td>
  </tr>
  <tr>
    <td>R</td>
    <td>R OUT</td>
  </tr>
  <tr>
    <td>FEAT 1</td>
    <td>BLANK</td>
  </tr>
  <tr>
    <td>FEAT 0</td>
    <td>FC1</td>
  </tr>
  <tr>
    <td>FCO</td>
    <td>G'/i</td>
  </tr>
  <tr>
    <td>B'/V</td>
    <td>HIN</td>
  </tr>
  <tr>
    <td>VIN</td>
    <td>14MHz</td>
  </tr>
  <tr>
    <td>Internal</td>
    <td>EXT OSC</td>
  </tr>
  <tr>
    <td>V OUT</td>
    <td>HOUT</td>
  </tr>
  <tr>
    <td>GND</td>
    <td>+5V</td>
  </tr>
</table>

Feature Connector Diagram
Specifications

System Board Switches

The following figure shows the proper system board DIP switch settings for the IBM Enhanced Graphics Adapter when used with the Personal Computer and the Personal Computer XT. The switch block locations are illustrated in the Technical Reference Manual "System Board Component Diagram". The Personal Computer has two DIP switch blocks; the switch settings shown pertain to DIP Switch Block 1. The Personal Computer XT has one DIP switch block.

![Diagram showing DIP switch block positions for the IBM Enhanced Graphics Adapter](page_324_670_598_120.png)

Switch Block (1)

Note: The DIP switches must be set as shown whenever the IBM Enhanced Graphics Adapter is installed, regardless of display type. This is true even when a second display adapter is installed in the system.
Configuration Switches

The following diagram shows the location and orientation of the configuration switches on the Enhanced Graphics Adapter.

Optional Graphics Memory Expansion Card

Option Retaining Bracket

Off    On
Configuration Switch Settings

The configuration switches on the Enhanced Graphics Adapter determine the type of display support the adapter provides, as follows:

<table>
  <tr>
    <th rowspan="2">SW1</th>
    <th rowspan="2">SW2</th>
    <th rowspan="2">SW3</th>
    <th rowspan="2">SW4</th>
    <th colspan="3">Configuration</th>
  </tr>
  <tr>
    <th>Enhanced Adapter</th>
    <th>Monochrome Adapter</th>
    <th>Color/Graphics Adapter</th>
  </tr>
  <tr>
    <td>On</td>
    <td>Off</td>
    <td>Off</td>
    <td>On</td>
    <td>Color Display<br>40x25</td>
    <td>Secondary</td>
    <td>–</td>
  </tr>
  <tr>
    <td>Off</td>
    <td>Off</td>
    <td>Off</td>
    <td>On</td>
    <td>Color Display<br>80x25</td>
    <td>Secondary</td>
    <td>–</td>
  </tr>
  <tr>
    <td>On</td>
    <td>On</td>
    <td>On</td>
    <td>Off</td>
    <td>Enhanced Display Emulation Mode</td>
    <td>Secondary</td>
    <td>–</td>
  </tr>
  <tr>
    <td>Off</td>
    <td>On</td>
    <td>On</td>
    <td>Off</td>
    <td>Enhanced Display Hi Res Mode</td>
    <td>Secondary</td>
    <td>–</td>
  </tr>
  <tr>
    <td>On</td>
    <td>Off</td>
    <td>On</td>
    <td>Off</td>
    <td>Monochrome</td>
    <td>–</td>
    <td>Secondary<br>40x25</td>
  </tr>
  <tr>
    <td>Off</td>
    <td>Off</td>
    <td>On</td>
    <td>Off</td>
    <td>Monochrome</td>
    <td>–</td>
    <td>Secondary<br>80x25</td>
  </tr>
</table>
<table>
  <tr>
    <th rowspan="2">SW1</th>
    <th rowspan="2">SW2</th>
    <th rowspan="2">SW3</th>
    <th rowspan="2">SW4</th>
    <th colspan="3">Configuration</th>
  </tr>
  <tr>
    <th>Enhanced Adapter</th>
    <th>Monochrome Adapter</th>
    <th>Color/Graphics Adapter</th>
  </tr>
  <tr>
    <td>On</td>
    <td>On</td>
    <td>On</td>
    <td>On</td>
    <td>Color Display<br>40x25</td>
    <td>Primary</td>
    <td>–</td>
  </tr>
  <tr>
    <td>Off</td>
    <td>On</td>
    <td>On</td>
    <td>On</td>
    <td>Color Display<br>80x25</td>
    <td>Primary</td>
    <td>–</td>
  </tr>
  <tr>
    <td>On</td>
    <td>Off</td>
    <td>On</td>
    <td>On</td>
    <td>Enhanced Display Emulation Mode</td>
    <td>Primary</td>
    <td>–</td>
  </tr>
  <tr>
    <td>Off</td>
    <td>Off</td>
    <td>On</td>
    <td>On</td>
    <td>Enhanced Display Hi Res Mode</td>
    <td>Primary</td>
    <td>–</td>
  </tr>
  <tr>
    <td>On</td>
    <td>On</td>
    <td>Off</td>
    <td>On</td>
    <td>Monochrome</td>
    <td>–</td>
    <td>Primary<br>40x25</td>
  </tr>
  <tr>
    <td>Off</td>
    <td>On</td>
    <td>Off</td>
    <td>On</td>
    <td>Monochrome</td>
    <td>–</td>
    <td>Primary<br>80x25</td>
  </tr>
</table>
Direct Drive Connector

9-Pin Direct Drive Signal

<table>
  <tr>
    <th>Signal Name · Description</th>
    <th>Pin</th>
    <th></th>
  </tr>
  <tr>
    <td>Ground</td>
    <td>1</td>
    <td rowspan="9">Enhanced Graphics Adapter</td>
  </tr>
  <tr>
    <td>Secondary Red</td>
    <td>2</td>
  </tr>
  <tr>
    <td>Primary Red</td>
    <td>3</td>
  </tr>
  <tr>
    <td>Primary Green</td>
    <td>4</td>
  </tr>
  <tr>
    <td>Primary Blue</td>
    <td>5</td>
  </tr>
  <tr>
    <td>Secondary Green/Intensity</td>
    <td>6</td>
  </tr>
  <tr>
    <td>Secondary Blue/Mono Video</td>
    <td>7</td>
  </tr>
  <tr>
    <td>Horizontal Retrace</td>
    <td>8</td>
  </tr>
  <tr>
    <td>Vertical Retrace</td>
    <td>9</td>
  </tr>
</table>
Light Pen Interface

P-2 Connector

<table>
  <tr>
    <th>P-2 Connector</th>
    <th>Pin</th>
    <th></th>
  </tr>
  <tr>
    <td rowspan="6">Light Pen Attachment</td>
    <td>+Light Pen Input</td>
    <td>1</td>
    <td rowspan="6">Enhanced Graphics Adapter</td>
  </tr>
  <tr>
    <td>Not used</td>
    <td>2</td>
  </tr>
  <tr>
    <td>+Light Pen Switch</td>
    <td>3</td>
  </tr>
  <tr>
    <td>Ground</td>
    <td>4</td>
  </tr>
  <tr>
    <td>+5 Volts</td>
    <td>5</td>
  </tr>
  <tr>
    <td>12 Volts</td>
    <td>6</td>
  </tr>
</table>
Jumper Descriptions

Located on the adapter are two jumpers designated P1 and P3. Jumper P1 changes the function of pin 2 on the direct drive interface. When placed on pins 2 and 3, jumper P1 selects ground as the function of direct drive interface, pin 2. This selection is for displays that support five color outputs, such as the IBM Color Display. When P1 is placed on pins 1 and 2, red prime output is placed on pin 2 of the direct drive interface connector. This supports the IBM Enhanced Color Display, which utilizes six color outputs on the direct drive interface.

Jumper P3 changes the I/O address port of the Enhanced Graphics Adapter within the system. In its normal position, (pins 1 and 2), all Enhanced Graphics Adapter addresses are in the range 3XX. Moving jumper P3 to pins 2 and 3 changes the addresses to 2XX. Operation of the adapter in the 2XX mode is not supported in BIOS.

The following figure shows the location of the jumpers and numbering of the connectors.

![Diagram showing the location of jumpers P1 and P3 on the Enhanced Graphics Adapter](page_489_670_579_246.png)
Logic Diagrams

ENHANCED GRAPHICS ADAPTER

Enhanced Graphics Adapter Sheet 1 of 11
Enhanced Graphics Adapter Sheet 2 of 11
SHT 2 MUX
SHT 2 MUX
SHT 2 CRT/cPU
SHT 2 -CLK
SHT 6 GRAPHICS
SHT 5 SREFADR
SHT 1 RESET
SHT 4 CRITOR
SHT 4 CRITOW
SHT 4 AD
SHT 1 BD0
SHT 1 BD1
SHT 1 BD2
SHT 1 BD3
SHT 1 BD4
SHT 1 BD5
SHT 1 BD6
SHT 1 BD7
SHT 9 LPSTB
SHT 1 BLANK
SHT 1 CURSORINT
SHT 1 REF AGR
SHT 1 SYNC
SHT 1 HRTC
SHT 1 VDO
SHT 1 DE
SHT 1 CURSORUL
SHT 1 CRTINT
SHT 1 BAO
SHT 1 BA1
SHT 1 BA2
SHT 1 BA3
SHT 1 BA4
SHT 1 BA5
SHT 1 BA6
SHT 1 BA7
SHT 1 RS0
SHT 1 RS1
SHT 1 RS2
SHT 1 RS3

CRT CNTL
MA0 10 10 19 AA0
MA1 18 20 19 AA1
MA2 17 20 19 AA2
MA3 14 19 AA3
MA4 14 19 AA4
MA5 7 19 AA5
MA6 7 19 AA6
MA7 7 19 AA7
LATCH/CLK

LS374
MA0 10 10 19 AA0
MA1 18 20 19 AA1
MA2 17 20 19 AA2
MA3 14 19 AA3
MA4 14 19 AA4
MA5 7 19 AA5
MA6 7 19 AA6
MA7 7 19 AA7
LATCH/CLK

LS374
MA0 10 10 19 BA0
MA1 18 20 19 BA1
MA2 17 20 19 BA2
MA3 14 19 BA3
MA4 14 19 BA4
MA5 7 19 BA5
MA6 7 19 BA6
MA7 7 19 BA7
LATCH/CLK

LS374
MA0 10 10 19 BA0
MA1 18 20 19 BA1
MA2 17 20 19 BA2
MA3 14 19 BA3
MA4 14 19 BA4
MA5 7 19 BA5
MA6 7 19 BA6
MA7 7 19 BA7
LATCH/CLK

Enhanced Graphics Adapter Sheet 3 of 11
ENHANCED GRAPHICS ADAPTER

Enhanced Graphics Adapter Sheet 4 of 11
SHT 3
RS0
RS1
RS2
RS3
RS4
RS5
RS6
RS7
MOD0
MOD1
MOD2

SHT 2 CRT LATCH

SHT 6 MOD0
SHT 6 MOD1
SHT 6 MOD2
SHT 6 MOD3

SHT 6 GRAPHICS

SHT 2 MUX

SHT 3 REF ADR
SHT 2 CCLK

LS374
18 1D 10 BA0
17 20 U12 16 BA2
16 40 U12 15 BA3
15 60 U12 14 BA4
14 80 U12 13 BA5
13 100 U12 12 BA6
12 120 U12 11 BA7
11 140 U12 OE
10 160 U12 CLK
9 180 U12 OE

LS374
18 1D 10 BA0
17 20 U12 16 BA2
16 40 U12 15 BA3
15 60 U12 14 BA4
14 80 U12 13 BA5
13 100 U12 12 BA6
12 120 U12 11 BA7
11 140 U12 OE
10 160 U12 CLK
9 180 U12 OE

LS0N U26
13 LS0N 12
2 LS10 U5 12
13 LS10 12
L6

U27
2 LS511 12
1 LS500 Ub 3
13 LS500 12
Ub
U6
U11

+5
10 74LS157H
12 PME Q 9
11 U1B Q 9
11 CLK Q 9
+5

SHT 7
BA0
BA1
BA2
BA3
BA4
BA5
BA6
BA7

SHT 8
GRAPHICS SHT 8
SREF ADR SHT 3

Enhanced Graphics Adapter Sheet 5 of 11
92 IBM Enhanced Graphics Adapter

August 2, 1984

ENHANCED GRAPHICS ADAPTER

Enhanced Graphics Adapter Sheet 6 of 11
ENHANCED GRAPHICS ADAPTER

Enhanced Graphics Adapter Sheet 7 of 11
SHT 6   CO
SHT 6   MID0
SHT 6   MID1
SHT 6   MID2
SHT 7   C2
SHT 7   MID3
SHT 7   C9
SHT 6   MID3
SHT 5   GRAPHICS
SHT 6   MID4
SHT 6   MID5
SHT 6   MID6
SHT 6   MID7
SHT 7   M200
SHT 7   M201
SHT 7   M202
SHT 7   M203
SHT 7   M204
SHT 7   M205
SHT 7   M206
SHT 7   M207
SHT 5   IE
SHT 1   BD0
SHT 1   BD1
SHT 1   BD2
SHT 1   BD3
SHT 1   BD4
SHT 1   BD5
SHT 2   CRT LATCH
SHT 2   ATR5/C2
SHT 2   DOT CLK
SHT 3   BLANK
SHT 3   VSYNC
SHT 3   CURSOR/UL
SHT 4   ATRIOR
SHT 4   ATRIOW

LS157

SEL
STB

ATTRIBUTE LSI
ATR0/CO
ATR1/C1
ATR2/C2
ATR3/C3
ATR4
ATR5
ATR6
ATR7
CC0
CC1
CC2
CC3
CC4
CC5
CC6
CC7
L6
D0
D1
D2
D3
D4
D5
C1/C2
DOTCLK
BLANK
VRTC
CURSOR
IOR
IOW

VCC
+5V
GND
20

Enhanced Graphics Adapter Sheet 8 of 11
ENHANCED GRAPHICS ADAPTER

Enhanced Graphics Adapter Sheet 9 of 11
SHT 9   FCO   21
SHT 9   FCI   20
SHT 8   R     17
         G     12
         B     14
         R'    13
         G'/I  22
SHT 8   BY/V  23
SHT 1   14MHZ 26
SHT 9   HINT  24
SHT 9   VEN   25
SHT 3   BLANK 18
SHT 2   ATRS/L 9
         +5V   27
SHT 9   INTERVAL 3
         +12V   2
         -12V   31
GND     7

NOTE:
1 GROUNDS-ONE AT EACH END OF CONNECTOR.

J2 EXT VIDEO

FEATURE CONNECTOR

J4

J1 VIDEO JACK

FEAT 0 SHT 9
FEAT 1 SHT 9
EXT OSC SHT 2
R OUT SHT 9
G OUT
B OUT
R' OUT
G'/OUT
B'/OUT
H OUT
V OUT
SHT 9

Enhanced Graphics Adapter Sheet 10 of 11
ENHANCED GRAPHICS ADAPTER

![Diagram of the Enhanced Graphics Adapter pinout, connector, and memory expansion.](page_384_698_1027_583.png)
ENHANCED GRAPHICS ADAPTER

Graphics Memory Expansion Card Sheet 1 of 5
ENHANCED GRAPHICS ADAPTER

Graphics Memory Expansion Card Sheet 2 of 5
Graphics Memory Expansion Card Sheet 3 of 5
ENHANCED GRAPHICS ADAPTER

![Block diagram of the Enhanced Graphics Adapter showing connections between U5, U6, U22, U23, and various signal lines labeled SW1 through SW7](page_184_320_1012_1202.png)

Graphics Memory Expansion Card Sheet 4 of 5
ENHANCED GRAPHICS ADAPTER

![Block diagram of the Enhanced Graphics Adapter circuit, showing connections between various chips and logic.](page_186_320_1017_1536.png)
BIOS Listing

Vectors with Special Meanings

Interrupt Hex 42 - Reserved

When an IBM Enhanced Graphics Adapter is installed, the BIOS routines use interrupt 42 to revector the video pointer.

Interrupt Hex 43 - IBM Enhanced Graphics Video Parameters

When an IBM Enhanced Graphics Adapter is installed, the BIOS routines use this vector to point to a data region containing the parameters required for the initializing of the IBM Enhanced Graphics Adapter. Note that the format of the table must adhere to the BIOS conventions established in the listing. The power-on routines initialize this vector to point to the parameters contained in the IBM Enhanced Graphics Adapter ROM.

Interrupt Hex 44 - Graphics Character Table

When an IBM Enhanced Graphics Adapter is installed the BIOS routines use this vector to point to a table of dot patterns that will be used when graphics characters are to be displayed. This table will be used for the first 128 code points in video modes 4, 5, and 6. This table will be used for 256 characters in all additional graphics modes. See the appropriate BIOS interface for additional information on setting and using the graphics character table pointer.
PAGE_120
FILE: ENHANCED GRAPHICS ADAPTER BIOS
EXTRN COMM_NEAR, OCDDST_NEAR, INT_1F_1_NEAR, COMM_FDC_NEAR
EXTRN END_ADDRESS_NEAR

THE BIOS ROUTINES ARE MEANT TO BE ACCESSED THROUGH SOFTWARE INTERRUPTS AND ADDRESSES DESCRIBED IN THE LISTINGS ARE INCLUDED ONLY FOR COMPLETENESS NOT FOR REFERENCE. APPLICATIONS WHICH REFERENCE THESE ROUTINES WILL PROBABLY SEGMENT VIOLATE THE STRUCTURE AND DESIGN OF BIOS.

.LIST
INCLUDE VFRONT.INC
SUBTL VFRONT.INC
PAGE

--- INT 10 -----------------------------

VIDEO_0
THESE ROUTINES PROVIDE THE CRT INTERFACE
THE FOLLOWING FUNCTIONS ARE PROVIDED:
(AH)=0 SET MODE (AL) CONTAINS MODE VALUE

<table>
  <tr>
    <th>AL</th>
    <th>AD</th>
    <th>TYPE</th>
    <th>RES</th>
    <th>NOTES</th>
    <th>DF-DIM</th>
    <th>DISPLAY</th>
    <th>MAX FCS</th>
  </tr>
  <tr>
    <td>*</td>
    <td>0</td>
    <td>B8</td>
    <td>ALPHA</td>
    <td>640X200</td>
    <td>40X25</td>
    <td>COLOR - BM</td>
    <td>8</td>
  </tr>
  <tr>
    <td>*</td>
    <td>1</td>
    <td>B8</td>
    <td>ALPHA</td>
    <td>640X200</td>
    <td>80X25</td>
    <td>COLOR - BM</td>
    <td>8</td>
  </tr>
  <tr>
    <td>*</td>
    <td>2</td>
    <td>B8</td>
    <td>ALPHA</td>
    <td>640X200</td>
    <td>80X25</td>
    <td>COLOR - BM</td>
    <td>8</td>
  </tr>
  <tr>
    <td>*</td>
    <td>3</td>
    <td>B8</td>
    <td>ALPHA</td>
    <td>640X200</td>
    <td>80X25</td>
    <td>COLOR - BM</td>
    <td>8</td>
  </tr>
  <tr>
    <td>*</td>
    <td>4</td>
    <td>B8</td>
    <td>GRPHX</td>
    <td>320X200</td>
    <td>40X25</td>
    <td>COLOR - BM</td>
    <td>1</td>
  </tr>
  <tr>
    <td>*</td>
    <td>5</td>
    <td>B8</td>
    <td>GRPHX</td>
    <td>320X200</td>
    <td>40X25</td>
    <td>COLOR - BM</td>
    <td>1</td>
  </tr>
  <tr>
    <td>*</td>
    <td>6</td>
    <td>B8</td>
    <td>GRPHX</td>
    <td>640X200</td>
    <td>80X25</td>
    <td>COLOR - BM</td>
    <td>1</td>
  </tr>
  <tr>
    <td>*</td>
    <td>7</td>
    <td>B8</td>
    <td>GRPHX</td>
    <td>640X200</td>
    <td>80X25</td>
    <td>MONOCHROME</td>
    <td>8</td>
  </tr>
  <tr>
    <td>*</td>
    <td>8</td>
    <td>RESERVED</td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td>*</td>
    <td>9</td>
    <td>RESERVED</td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td>A</td>
    <td>RESERVED</td>
    <td>INTERNAL USE</td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
  <tr>
    <td>C</td>
    <td>RESERVED</td>
    <td>INTERNAL USE</td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
    <td></td>
  </tr>
</table>

NOTE : HIGH BIT AL SET PREVENTS REGCN BUFFER CLEAR ON MODES RUNNING ON THE COMBO VIDEO ADAPTER

*** NOTE BY MODES OPERATE SAME AS COLOR MODES, BUT COLOR ARGUMENT IS NOT ENCODED

(AH)=1 SET CURSOR TYPE
(CH) = BIT 0-0 = START LINE FOR CURSOR
BIT 1 = HARDWARE WILL ALWAYS CAUSE BLINK
** SETTING BIT 3 OR 4 WILL CAUSE ERRATIC CURSOR AT (0,0)
(CF) = BITS 4-0 = END LINE FOR CURSOR
(OH,DL) = ROW,COLUMN (0,0) IS UPPER LEFT
(BX) = PAGE NUMBER
(AH)=2 SET CURSOR POSITION
(BH) = PAGE NUMBER
ON EXIT (DH,DL) = ROW,COLUMN OF CURRENT CURSOR
(AH)=3 READ LIGHT PEN POSITION
(AH) = 0 -- LIGHT PEN SWITCH NOT DOWN/NOT TRIGGERED
(AH) = 1 -- VALID LIGHT PEN VALUE IN REGISTER
(AX) = LIGHT PEN X COORDINATE POSN
(CH) = RASTER LINE (0-199)
(CX) = RASTER COLUMN (0-199)
(BX) = RASTER COLUMN (0-199,599)
(AH)=5 SELECT ACTIVE DISPLAY PAGE
(AH) = 0 -- DEFAULT SEE AH=0 FOR PAGE INFO
(AH)=6 SCROLL ACTIVE PAGE UP
(AL) = NUMBER OF LINES, INPUT LINES BLANKED AT BOTTOM
AL = 0 MEANS BLANK ENTIRE WINDOW
(CH,CL) = ROW,COLUMN OF UPPER LEFT CORNER OF SCROLL
(DH,DL) = ROW,COLUMN OF LOWER RIGHT CORNER OF SCROLL
(BH) = ATTRIBUTE TO BE USED ON BLANK LINE
(AH)=7 SCROLL ACTIVE PAGE DOWN
(AL) = NUMBER OF LINES, INPUT LINES BLANKED AT TOP
AL = 0 MEANS BLANK ENTIRE WINDOW
(CH,CL) = ROW,COLUMN OF UPPER LEFT CORNER OF SCROLL
(DH,DL) = ROW,COLUMN OF LOWER RIGHT CORNER OF SCROLL
(BH) = ATTRIBUTE TO BE USED ON BLANK LINE

CHARACTER HANDLING ROUTINES

(AH) = 8 READ ATTRIBUTE/CHARACTER AT CURRENT CURSOR POSITION
(BH) = DISPLAY PAGE
ON EXIT
(AH) = CHAR READ
(AH) = ATTRIBUTE OF CHARACTER READ (ALPHA MODES ONLY)
(AH) = 9 WRITE ATTRIBUTE/CHARACTER AT CURRENT CURSOR POSITION
(CX) = COUNT OF CHARACTERS TO WRITE
(AH) = 10 WRITE CHARACTER ONLY AT CURRENT CURSOR POSITION
(BL) = ATTRIBUTE OF CHARACTER (ALPHA)/COLOR OF CHAR (GRAPHICS)
(AH) = A WRITE CHARACTER ONLY AT CURRENT CURSOR POSITION
(BH) = DISPLAY PAGE
(CX) = COUNT OF CHARACTERS TO WRITE
(AH) = 11 CHAR TO WRITE
FOR READ/WRITE CHARACTER INTERFACE WHILE IN GRAPHICS MODE, THE CHARACTERS ARE FORMED FROM A CHARACTER GENERATOR IMAGE MAINTAINED IN THE SYSTEM ROM. ONLY 151-128 CHARS ARE CONTAINED THERE. TO READ ALL THE SECOND 128 CHAR, THE USER MUST INITIALIZE THE POINTER AT INTERRUPT VECTOR LOCATION POINTING TO POLARIS' 1K BYTE TABLE CONTAINING THE CODE POINTS FOR THE SECOND 128 CHARS (128-255).
FOR THE NEW GRAPHICS MODES 256 GRAPHICS CHAR ARE SUPPLIED IN THE SYSTEM ROM.
FOR WRITE CHARACTER INTERFACE IN GRAPHICS MODE, THE REPLICATION OF CHARACTERS CONTAINED IN LOC ENTRY WILL PRODUCE VALID RESULTS ONLY FOR CHARACTERS CONTAINED ON THE SAME ROW. CONTINUATION TO SUCCEEDING LINES WILL NOT PRODUCE CORRECTLY.
GRAPHICS INTERFACE

(AH) = B SET COLOR PALETTE
FOR 80X25 COMPATIBILITY MODES
(BH) = PALETTE COLOR ID BEING SET (0-127)
(BL) = COLOR VALUE TO BE USED WITH THAT COLOR ID
NOTE: FOR THE CURRENT COLOR CARD, THIS ENTRY POINT HAS MEANING ONLY FOR 320X200 GRAPHICS COLOR MODES. FOR OTHER MODES, COLOR ID (0-15):
COLOR ID = 1 SELECTS THE PALETTE TO BE USED:
0 = CYAN/BLUE/RED/BLACK
1 = CYAN/BLUE/GREEN/WHITE
IN 40X25 OR 80X25 ALPHA MODES, THE VALUE SET BY COLOR ID 1 WILL BE USED AS THE BORDER COLOR TO BE USED [VALUES 0-31, WHERE 16-31 SELECT THE HIGH INTENSITY BACKGROUND SET].

(AH) = C WRITE DOT
(BH) = COLOR
(DX) = ROW NUMBER
(CX) = COLUMN NUMBER
(AL) = COLOR VALUE
IF BIT 7 OF AL = 1, THEN THE COLOR VALUE IS EXCLUSIVE OR'D WITH THE CURRENT CONTENTS OF THE DOT.

(AH) = D READ DOT
(BH) = COLOR
(DX) = ROW NUMBER
(CX) = COLUMN NUMBER
(AL) RETURNS THE DOT READ

ASCII TELETEYPE ROUTINE FOR OUTPUT

(AH) = E WRITE TELETEYPE TO ACTIVE PAGE
(BH) = FOREGROUND COLOR IN GRAPHICS MODE
(BL) = FOREGROUND COLOR IN GRAPHICS MODE
NOTE -- SCREEN WIDTH IS CONTROLLED BY PREVIOUS MODE SET

(AH) = F CURRENT VIDEO STATE
RETURNS CURRENT VIDEO STATE
(AL) = MODE CURRENTLY SET (SEE AH=0 FOR EXPLANATION)
(AH) = NUMBER OF CHARACTER COLUMNS ON SCREEN
(BH) = CURRENT ACTIVE DISPLAY PAGE

(AH) = 10 SET PALETTE REGISTERS
(AL) = 0 SET INDIVIDUAL PALETTE REGISTER
BL = PALETTE REGISTER TO BE SET
BH = VALUE TO SET

AL = 1 SET OVERSCAN REGISTER
BH = VALUE TO SET

AL = 2 SET ALL PALETTE REGISTERS AND OVERSCAN
ES:DX POINTS TO A 17 BYTE TABLE
BYTE 15 IS THE PALETTE VALUES, RESPECTIVELY
BYTE 16 IS THE OVERSCAN VALUE

AL = 3 TOGGLE INTENSIFY/BLINKING BIT
BL = 0 ENABLE INTENSIFY
BL = 1 ENABLE BLINKING

(AH) = 11 CHARACTER GENERATOR ROUTINE
NOTE : THIS ROUTINE DOES NOT CHANGE A MODE SET, COMPLETELY RESETTING THE VIDEO ENVIRONMENT BUT MAINTAINING THE REGEN BUFFER.

AL = 00 USER A H/A LOAD.
ES:BP = POINTS TO USER TABLE
CX = COUNT TO STORE
DX = CHARACTER OFFSET INTO TABLE
BL = BLOCK TO LOAD
BH = NUMBER OF BYTES PER CHARACTER

AL = 01 ROM MODE LOAD
BL = BLOCK TO LOAD

AL = 02 ROM 8X8 DOUBLE DOT
BL = BLOCK TO LOAD

AL = 03 SET BLOCK SPECIFIER
BL = CHAR GEN BLOCK SPECIFIER
D1-D0 ATTR BIT 3 ONE, CHAR GEN 0-3
D1-D0 ATTR BIT 3 ZERO, CHAR GEN 0-3
NOTE : MODE USR00H = 03 A FUNCTION CALL
AX = 1000H
BX = 0712H
IS RECOMMENDED TO SET THE COLOR PLANES RESULTING IN 512 CHARACTERS AND EIGHT CONSISTENT COLORS.

NOTE : THE FOLLOWING INTERFACE (AL=1X) IS SIMILAR IN FUNCTION TO (AL=0X) EXCEPT:

- PAGE ZERO MUST BE ACTIVE
- POINTS (BYTES/COLUMNS) MUST BE RECALCULATED
- ROWS WILL BE CALCULATED FROM THE FOLLOWING:
  INT(200 OR 350) POINTS - 1
  CRT_COLS * CRT_ROWS + 1
  (ROWS + 1) * CRT_COLS * 2
- THE CRT WILL BE REPROGRAMMED AS FOLLOWS :
  R09H DONE ONLY IN MODE 7
  ROAH = POINTS - 2 CURSOR START
  ROBH = CURSOR END
  R12H = VERT DISP END
  R14H = POINTS UNDERLINE LOC

THE ABOVE REGISTER CALCULATIONS MUST BE CLOSE TO THE ORIGINAL TABLE VALUES OR UNDETERMINED RESULTS WILL OCCUR.

NOTE : THE FOLLOWING INTERFACE IS DESIGNED TO BE CALLED ONLY IMMEDIATELY AFTER A MODE SET HAS BEEN ISSUED. FAILURE TO ADHERE TO THIS PRACTICE MAY CAUSE UNDETERMINED RESULTS.

AL = 10 USER A H/A LOAD.
ES:BP = POINTS TO USER TABLE
CX = COUNT TO STORE
DX = CHARACTER OFFSET INTO TABLE
BL = BLOCK TO LOAD
BH = NUMBER OF BYTES PER CHARACTER

AL = 11 ROM MODE LOAD
BL = BLOCK TO LOAD

AL = 12 ROM 8X8 DOUBLE DOT
BL = BLOCK TO LOAD
NOTE : THE FOLLOWING INTERFACE IS DESIGNED TO BE CALLED ONLY IMMEDIATELY AFTER A MODE SET HAS BEEN ISSUED. FAILURE TO ADHERE TO THIS PRACTICE MAY CAUSE UNDETERMINED RESULTS.

AL = 20 USER GRAPHICS CHARs INT 01FH (8x8)
ES:BP - POINTER TO USER TABLE
AL = 21 USER GRAPHICS CHARs
ES:BP - POINTER TO USER TABLE
- ROW SPECIFIER (CHARS PER CHARACTER)
BL - ROW SPECIFIER

BL = 0 USER
DL = ROWS
BL = 1 (20H)
BL = 2 25 (19H)
BL = 3 43 (2BH)

AL = 22 ROM 8 X 14 SET
ROM SELECTOR
AL = 23 ROM 8 X 8 DOUBLE DOT
ROM SELECTOR
- ROW SPECIFIER

AL = 30 INFORMATION
CX - POINTS
DL - ROWS
BH = 0 RETURN CURRENT INT 17H PTR
ES:BP - PTR TO TABLE
BH = 1 RETURN CURRENT INT 4AH PTR
ES:BP - PTR TO TABLE
BH = 2 RETURN ROM 8 X 14 PTR
ES:BP - PTR TO TABLE
BH = 3 RETURN ROM DOUBLE DOT PTR
ES:BP - PTR TO TABLE
BH = 4 RETURN ROM DOUBLE DOT PTR (TOP)
ES:BP - PTR TO TABLE
BH = 5 RETURN ROM ALPHA ALTERNATE 9X14
ES:BP - PTR TO TABLE

(AH) = 12 ALTERNATE SELECT
BL = 10 RETURN EGA INFORMATION
BH = 0 COLOR MODE IN EFFECT <3><3><3>
1 MONOC MODE IN EFFECT <3><3><3>
BL = MEMORY VALUE
0 0 064K C 1 128K
1 0 192K C 1 256K
CH = FLEXIBLE BITMAP
CL = SWITCH SETTING

BL = 20 SELECT ALTERNATE PRINT SCREEN ROUTINE

(AH) = 13 WRITE STRING
ES:BP - POINTER TO STRING TO BE WRITTEN
CX - CHARACTER ONLY COUNT
DX - LOCATION TO BEGIN STRING, IN CURSOR TERMS
BH - PAGE NUMBER

AL = 0
BL - ATTRIBUTE
STRING - (CHAR, CHAR, CHAR, ...)
CURSOR NOT MOVED

AL = 1
BL - ATTRIBUTE
STRING - (CHAR, CHAR, CHAR, ...)
CURSOR IS MOVED

AL = 2
STRING - (CHAR, ATTR, CHAR, ATTR, ...)
CURSOR NOT MOVED

AL = 3
STRING - (CHAR, ATTR, CHAR, ATTR, ...)
CURSOR IS MOVED

NOTE : CHAR RET, LINE FEED, BACKSPACE, AND BELL ARE TREATED AS COMMANDS RATHER THAN PRINTABLE CHARACTERS.

SRLOAD MACRO SEGREG, VALUE
IF IDN <VALUE>,<0>
SUB DX,DX
ELSE
MOV DX,VAL\VE
ENDIF
ENDM
SEGREG,DX
ENDM

;------ LOW MEMORY SEGMENT

ABSO SEGMENT AT '0'
ORG 005H*4 ; PRINT SCREEN VECTOR
INT2_PTR LABEL DDWORD
VIDEO ORG 017H*4 ; VIDEO I/O VECTOR
LABEL DDWORD
EXT_PTR ORG 017H*4 ; GRAPHIC CHARS 128-255
LABEL DDWORD
ORG 028H*4 ; REVECTORED 10H*4
PLANAR_VIDEO LABEL DDWORD
ORG 034H*4 ; GRAPHIC CHARS 0-255
GRX_SET LABEL DDWORD
ORG 0410H BYTE
EQUIP_L0W LABEL DW ?
EQUIP_FLAG DW ?
;------ REUSE RM FROM PLANAR BIOS
ORG 049H
CRT_MODE DB ?
CRT_COLS DB ?
CRT_ROWS DB ?
CRT_START DB ?
CURSOR_POSN DW 8 DUP(?)
CURSOR_MODE DW ?
ACTIVE_PAGE DB ?
0463 ????    379   ADDR_6845 DW ?
0465 ??      380   CRT_MODE_SET DB ?
0466 ??      381   CRT_PALETTE DB ?
0467 ??      382
0472 ??      383   RESET_FLAG ORG 0472H DB ?
0473 ??      384   ORG 0474H DB ?
0484 ??      385   ROWS DB ?
0485 ??      386   POINTS DB ? ; ROWS ON THE SCREEN
0486 ??      387   DB ? ; BYTES PER CHARACTER
0487 ??      388
0488 ??      389   INFO DB ?
0489 ??      390
0491 ??      391   ; INFO
0492 ??      392   ; D7 = HIGH BIT OF MODE SET, CLEAR/NOT CLEAR REGEN
0493 ??      393   ; D6 = MEMORY D6 = 0 - 064K 0 - 128K
0494 ??      394   ; D5 = 0 - 0 192K 1 - 256K
0495 ??      395   ; D4 = RESERVED
0496 ??      396   ; D3 = EGA ACTIVE MONITOR (0) VGA NOT ACTIVE (1)
0497 ??      397   ; D2 = VGA FOR DISPLAY ENABLED (1)
0498 ??      398   ; D1 = EGA HAS A MONOCHROME ATTACHED (1)
0499 ??      399   ; D0 = SET C_TYPE EMULATE ACTIVE (0)
0500 ??      400
0501 ??      401   INFO_3 DB ?
0502 ??      402
0503 ??      403   ; INFO_3
0504 ??      404   ; D7-D4 FEATURE BITS
0505 ??      405   ; D3-D0 SWITCHES
0506 ??      406
0507 ??      407   ORG 04A8H
0508 ??      408   SAVE_PTR LABEL DWORD
0509 ??      409
0510 ??      410   ------ SAVE_PTR
0511 ??      411   SAVE_PTR IS A POINTER TO A TABLE AS DESCRIBED AS FOLLOWS :
0512 ??      412
0513 ??      413   DWORD_1
0514 ??      414   VIDEO PARAMETER TABLE POINTER
0515 ??      415   DWORD_2
0516 ??      416   ALPHA MODE AUXILIARY CHAR GEN POINTER
0517 ??      417   GRAPHICS MODE AUXILIARY CHAR GEN POINTER
0518 ??      418   DWORD_3
0519 ??      419   RESERVED
0520 ??      420   RESERVED
0521 ??      421   RESERVED
0522 ??      422   PARAMETER TABLE POINTER
0523 ??      423   INITIALIZED TO 0000:0000. THIS VALUE IS OPTIONAL.
0524 ??      424   WHEN NON-ZERO, THIS POINTER IS USED AS A POINTER
0525 ??      425   TO A RAM AREA WHERE CERTAIN DYNAMIC VALUES ARE TO
0526 ??      426   BE SAVED. WHEN AN EGA OPERATION THIS RAM AREA WILL
0527 ??      427   BE USED TO SAVE EGA PARAMETERS SUCH AS OVERSCAN
0528 ??      428   THE OVERSCAN VALUE IN BYTES 0-160 RESPECTIVELY.
0529 ??      429   AT LEAST 256 BYTES MUST BE ALLOCATED FOR THIS AREA.
0530 ??      430
0531 ??      431   DWORD_3
0532 ??      432   ALPHA MODE AUXILIARY POINTER
0533 ??      433   INITIALIZED TO 0000:0000. THIS VALUE IS OPTIONAL.
0534 ??      434   WHEN NON-ZERO, THIS POINTER IS USED AS A POINTER
0535 ??      435   TO A TABLES DESCRIBED AS FOLLOWS :
0536 ??      436
0537 ??      437   BYTE BYTES/CHARACTER
0538 ??      438   BYTE COUNT TO LOAD, SHOULD BE ZERO FOR NORMAL
0539 ??      439   OPERATION
0540 ??      440   WORD COUNT TO STORE, SHOULD BE 2560 FOR NORMAL
0541 ??      441   OPERATION
0542 ??      442   WORD CHARACTER OFFSET, SHOULD BE ZERO FOR NORMAL
0543 ??      443   OPERATION
0544 ??      444   DWORD POINTER TO A FONT TABLE
0545 ??      445   BYTE DISPLAYABLE ROWS
0546 ??      446   BYTE MINIMUM CALCULATED VALUE WILL BE
0547 ??      447   USED, ELSE THIS VALUE WILL BE USED
0548 ??      448   CONSECUTIVE BYTES OF MODE VALUES FOR WHICH
0549 ??      449   THE DESCRIPTION IS TO BE USED.
0550 ??      450   THE END OF THIS STREAM IS INDICATED BY A
0551 ??      451   BYTE CODE OF 'FF'
0552 ??      452
0553 ??      453   NOTE : USE OF THIS POINTER MAY CAUSE UNEXPECTED
0554 ??      454   CURSOR LOCATION INFORMATION. FOR AN EXPLANATION
0555 ??      455   OF CURSOR TYPE SEE AH = 01 IN THE INTERFACE
0556 ??      456
0557 ??      457   DWORD_4
0558 ??      458   GRAPHICS MODE AUXILIARY POINTER
0559 ??      459   INITIALIZED TO 0000:0000. THIS VALUE IS OPTIONAL.
0560 ??      460   WHEN NON-ZERO, THIS POINTER IS USED AS A POINTER
0561 ??      461   TO A TABLES DESCRIBED AS FOLLOWS :
0562 ??      462
0563 ??      463   BYTE DISPLAYABLE ROWS
0564 ??      464   BYTE DISPLAYABLE CHARACTER
0565 ??      465   DWORD POINTER TO A FONT TABLE
0566 ??      466   BYTE CONSECUTIVE BYTES OF MODE VALUES FOR WHICH
0567 ??      467   THE DESCRIPTION IS TO BE USED.
0568 ??      468   THE END OF THIS STREAM IS INDICATED BY A
0569 ??      469   BYTE CODE OF 'FF'
0570 ??      470
0571 ??      471   DWORD_5 THRU DWORD_7
0572 ??      472   RESERVED AND SET TO 0000:0000.
0573 ??      473
0574 ??      474
0575 ??      475
0576 ??      476
0577 ??      477   ORG 0500H
0578 ??      478   STATUS_BYTE DB ?
0579 ??      479   ABSS ENDS
0580 ??      480
0581 ??      481
0582 ??      482   PORT_B EQU 61H ; 8255 PORT B ADDR
0583 ??      483   TIMER EQU 40H
0584 ??      484
0585 ??      485   ------ EQUATES FOR CARD PORT ADDRESSES
0586 ??      486   SEQ_ADDR EQU OC4H
0587 ??      487   SEQ_ADR EQU OC5H
0588 ??      488   CRTC_ADDR EQU OC4H
0589 ??      489   CRTC_DATA EQU OC5H ; OR OB5H
0590 ??      490   CRTC_DATA EQU OC5H
0591 ??      491   GRAP1_DATA EQU OCCH
0592 ??      492   GRAP1_POS EQU OCCH
0593 ??      493   GRAP1_ADDR EQU OCCH
0594 ??      494   GRAP1_DATA EQU OCCH
0595 ??      495   GRAP1_ADDR EQU OCCH
0596 ??      496   MISC_OUTPUT EQU OCCH
0597 ??      497   IN_STAT_0 EQU OC2H
0598 ??      498   INPUT_STATUS_B EQU ODAH
0599 ??      499   INPUT_STATUS EQU ODAH
0600 ??      500   ATTR_READ EQU ODAH
0601 ??      501   ATTR_WRITE EQU ODCH
0602 ??      502
0603 ??      503
0604 ??      504   ------ EQUATES FOR ADDRESS REGISTER VALUES
= 0000      505 C     S_RESET    EQU   00H
= 0001      506 C     S_CLOCK    EQU   01H
= 0002      507 C     S_RAF      EQU   02H
= 0003      508 C     S_CGEN     EQU   03H
= 0004      509 C     S_MEM      EQU   04H
= 0005      510 C
= 0006      511 C     C_HRZ_TOT  EQU   01H
= 0007      512 C     C_HRZ_DFT  EQU   02H
= 0008      513 C     C_HRZ_BLK  EQU   03H
= 0009      514 C     C_END_HRZ  EQU   04H
= 000A      515 C     C_VERT_TOT EQU   05H
= 000B      516 C     C_VERT_DFT EQU   06H
= 000C      517 C     C_VERT_BLK EQU   07H
= 000D      518 C     C_END_VERT EQU   08H
= 000E      519 C     C_PRE_ROW  EQU   09H
= 000F      520 C     C_CUR_ROW  EQU   0AH
= 0010      521 C     C_CHSR_START EQU   0BH
= 0011      522 C     C_CHSR_END  EQU   0CH
= 0012      523 C     C_STRETCH_HIGH EQU   0DH
= 0013      524 C     C_STRETCH_LOW EQU   0EH
= 0014      525 C     C_CHSR_LDC_HGH EQU   0FH
= 0015      526 C     C_CHSR_LDC_LW EQU   10H
= 0016      527 C     C_VRT_SYN_STAT EQU   11H WRITE ONLY
= 0017      528 C     C_VRT_SYN_END EQU   12H READ ONLY
= 0018      529 C     C_VRT_SYN_END EQU   13H WRITE ONLY
= 0019      530 C     C_LIGHTEN_LOW EQU   14H READ ONLY
= 001A      531 C     C_VRT_DSYN_END EQU   15H
= 001B      532 C     C_OFFSET    EQU   16H
= 001C      533 C     C_UNDERL_LDC EQU   17H
= 001D      534 C     C_STRT_VRT_BLK EQU   18H
= 001E      535 C     C_END_VRT_BLK EQU   19H
= 001F      536 C     C_GPOW      EQU   1AH
= 0020      537 C     C_LN_COMP   EQU   1BH

= 0000      538 C
= 0001      540 C     G_SET_RESET EQU   00H
= 0002      541 C     G_ENBL_SET  EQU   01H
= 0003      542 C     G_CLR_CMP   EQU   02H
= 0004      543 C     G_DATA_RST  EQU   03H
= 0005      544 C     G_READ_MA   EQU   04H
= 0006      545 C     G_MSC       EQU   05H
= 0007      546 C     G_MISC      EQU   06H
= 0008      547 C     G_COLOR     EQU   07H
= 0009      548 C     G_BK_MASK   EQU   08H
= 0010      549 C     P_MODE      EQU   10H
= 0011      550 C     P_OVERS     EQU   11H
= 0012      551 C     P_PLANE     EQU   12H
= 0013      552 C     P_PPEL      EQU   13H
= 0014      553 C     SUBTTL
= 0015      554 C
= 0016      555 C     ;------ CODE SEGMENT
= 0017      556 C     CODE SEGMENT PUBLIC
= 0018      557 C     INCLUDE VPOST.INC
= 0019      558 C     SUBTTL VPOST.INC
= 0020      559 C     PAGE
= 0021      560 C
= 0022      561 C     ;------ POST
= 0023      562 C     ASSUME CS:CODE,DS:ABSO
= 0024      563 C     ORG 0000H
= 0025      564 C     DB 055H ; SIGNATURE
= 0026      565 C     DB 0AAH ; BYTES
= 0027      566 C     DB 020H ; LENGTH INDICATOR
= 0028      567 C     ;------ NOTE : DO NOT USE THE SIGNATURE BYTES AS A PRESENCE TEST
= 0029      568 C     ; PLANNER VIDEO SWITCH SETTINGS
= 0030      569 C     ; 0 0 - UNUSED
= 0031      570 C     ; 0 1 - 40 X 25 COLOR
= 0032      571 C     ; 1 0 - 80 X 25 COLOR
= 0033      572 C     ; 1 1 - 80 X 25 MONOCHROME
= 0034      573 C     ; NOTE : 0 MUST BE SET WHEN THIS ADAPTER IS INSTALLED.
= 0035      574 C     ; VIDEO ADAPTER SWITCH SETTINGS
= 0036      575 C     ; 0 0 0 0 - MONOC PRIMARY, EGA COLOR, 40X25
= 0037      576 C     ; 0 0 0 1 - MONOC PRIMARY, EGA COLOR, 80X25
= 0038      577 C     ; 0 0 1 0 - MONOC PRIMARY, EGA HI RES EMULATE (SAME AS 0001)
= 0039      578 C     ; 0 0 1 1 - MONOC PRIMARY, EGA HI RES ENHANCED
= 0040      579 C     ; 0 1 0 0 - MONOC PRIMARY, EGA MONOCHROME
= 0041      580 C     ; 0 1 0 1 - COLOR 80 PRIMARY, EGA MONOCHROME
= 0042      581 C     ; 0 1 1 0 - MONOC SECONDARY, EGA COLOR, 40X25
= 0043      582 C     ; 0 1 1 1 - MONOC SECONDARY, EGA COLOR, 80X25
= 0044      583 C     ; 1 0 0 0 - MONOC SECONDARY, EGA HI RES EMULATE (SAME AS 0111)
= 0045      584 C     ; 1 0 0 1 - MONOC SECONDARY, EGA HI RES ENHANCED
= 0046      585 C     ; 1 0 1 0 - COLOR 80 SECONDARY, EGA MONOCHROME
= 0047      586 C     ; 1 0 1 1 - COLOR 80 SECONDARY, EGA MONOCHROME
= 0048      587 C     ; 1 1 0 0 - RESERVED
= 0049      588 C     ; 1 1 0 1 - RESERVED
= 0050      589 C     ; 1 1 1 0 - RESERVED
= 0051      590 C     ; 1 1 1 1 - RESERVED
= 0052      591 C     ;------ SETUP ROUTINE FOR THIS MODULE
= 0053      592 C     VIDEO_SETUP PROC FAR
= 0054      593 C     JMP SHORT L1
= 0055      594 C     DB '2400'
= 0056      595 C     DB '6277356 (C)COPYRIGHT IBM 1984'
= 0057      596 C     DB '9/13/84'
= 0058      597 C
= 0059      598 C     ;------ SET UP VIDEO VECTORS
= 0060      599 C     L1:
= 0061      600 C     MOV DH,3
= 0062      601 C     MOV DL,INPUT_STATUS
= 0063      602 C     IN AL,DX
= 0064      603 C     MOV INPUT_STATUS_B
= 0065      604 C     IN AL,DX
= 0066      605 C     MOV ATTR_WRITE
= 0067      606 C     MOV DX,AL
= 0068      607 C     OUT DX,AL
= 0069      608 C     SRLOAD DS,0
= 0070      609 C     SRG DX,DX
= 0071      610 C     MOV DS,DX
003E FA
003F C7 06 0000 R OC7 R 631
0040 C7 06 0002 R F065 632
0041 C7 06 0108 R F065 633
0042 C7 06 0048 R 010C 634
0043 C7 06 0048 R 010C 635
0044 C7 06 0048 R 010C 636
0045 C7 06 004A R 010C 637
0046 C7 06 0000 E 0000 E 638
0047 C7 06 0000 E 0000 E 639
0048 C7 06 010C R 0000 E 640
0049 C7 06 010C R 0000 E 641
004A FB 642
004B 643
004C 644
004D 645
004E 646
004F 647
0050 648
0051 649
0052 650
0053 651
0054 652
0055 653
0056 654
0057 655
0058 656
0059 657
005A 658
005B 659
005C 660
005D 661
005E 662
005F 663
0060 664
0061 665
0062 666
0063 667
0064 668
0065 669
0066 670
0067 671
0068 672
0069 673
006A 674
006B 675
006C 676
006D 677
006E 678
006F 679
0070 680
0071 681
0072 682
0073 683
0074 684
0075 685
0076 686
0077 687
0078 688
0079 689
007A 690
007B 691
007C 692
007D 693
007E 694
007F 695
0080 696
0081 697
0082 698
0083 699
0084 700
0085 701
0086 702
0087 703
0088 704
0089 705
008A 706
008B 707
008C 708
008D 709
008E 710
008F 711
0090 712
0091 713
0092 714
0093 715
0094 716
0095 717
0096 718
0097 719
0098 720
0099 721
009A 722
009B 723
009C 724
009D 725
009E 726
009F 727
00A0 728
00A1 729
00A2 730
00A3 731
00A4 732
00A5 733
00A6 734
00A7 735
00A8 736
00A9 737
00AA 738
00AB 739
00AC 740
00AD 741
00AE 742
00AF 743
00B0 744
00B1 745
00B2 746
00B3 747
00B4 748
00B5 749
00B6 750
00B7 751
00B8 752
00B9 753
00BA 754
00BB 755
00BC 756

CL_I
MOV WORD PTR VIDEO_OFFSET COMBO_VIDEO
MOV WORD PTR VIDEO2_CS, CS
MOV WORD PTR PLANAR_VIDEO_OF065H
MOV WORD PTR PLANAR_VIDEO_OF065H
MOV WORD PTR SAVE_PTR, OFFSET SAVE_TBL
MOV WORD PTR SAVE_PTR+2, CS
MOV WORD PTR EXT_PTR+2, CS
MOV WORD PTR INT_OFFSET_INT_IF_1
MOV WORD PTR EXT_PTR+2, CS
MOV WORD PTR GRX_SET, OFFSET GDDOF
MOV WORD PTR GRX_SET+2, CS
STI

;----- POST FOR COMBO VIDEO CARD
MOV INFO,00000100B
CALL RD_SWS
MOV INT, BL
CALL F_BTS
CALL INFO_3_AL
OR INFO_3_AL
CALL MK_ENV
JMP POST

SKIP:
RET
VIDEO_SETUP ENDP

POR_1 PROC NEAR
OUT DX,AL
PUSH AX
POP AX
IM AL,DX
AND AL,010H
SHR AL,1
RET
POR_1 ENDP

;----- READ THE SWITCH SETTINGS ON THE CARD
RD_SWS PROC NEAR
COME DS:ABS0
MOV DH,3
MOV DL,MSC_OUTPUT
MOV AL,1
OUT DX,AL
;----- COULD BE 0,4,8,C
POR_1 PROC NEAR
OUT DX,AL
PUSH AX
POP AX
IM AL,DX
AND AL,010H
SHR AL,1
RET
POR_1 ENDP

;----- OBTAIN THE FEATURE BITS FROM DAUGHTER CARD
F_BTS PROC NEAR
MOV DH,3
MOV DL,OBAH
MOV AL,1
OUT DX,AL
MOV DL,ODAH
OUT DX,AL
MOV AL,IN_STAT_0
IH AL,DL
AND AL,060H ; READ FEATURE BITS
SHR AL,1
OR AL,1
; READ FEATURE BITS
F_BTS ENDP

MK_ENV PROC NEAR
ASSUME DS:ABS0
SUB SP,4
AND BL,0FH
SAL BX,1
PUSH BX
MOV DH,2
MOV AL,0OH
POP DX
POP AL
INC AH,1
NOT AH
WORD PTR CS:[BX + OFFSET T5]

SAVE_TBL LABEL DWORD
DW OFFSET VIDEO_PARMS ; PPARS
DW OFFSET VIDEO_PARMS ; PPARS
DW OFFSET SAVE_AREA ; PAL SAVE AREA
DW OFFSET SAVE_AREA ; PAL SAVE AREA
DW 0 ; ALPHA TABLES
DW 0 ; ALPHA TABLES
DW 0 ; GRAPHICS TABLES
DW 0 ; GRAPHICS TABLES
011A 0000    757 c   DW 0 ; GRAPHICS TABLES
011C 0000    758 c   DW 0
011E 0000    759 c   DW 0
0120 0000    760 c   DW 0
0122 0000    761 c   DW 0
0124 0000    762 c   DW 0
0126 0000    763 c   DW 0
0128 0000    764 c   DW 0
012A 0173 R  765 c   15 LABEL WORD
012A 017E R  766 c   OFFSET PST_0
012C 017F R  767 c   OFFSET PST_1
012E 0180 R  768 c   OFFSET PST_2
012E 0185 R  769 c   OFFSET PST_3
0130 0194 R  770 c   OFFSET PST_4
0132 0195 R  771 c   OFFSET PST_5
0134 01BC R  772 c   OFFSET PST_6
0134 01C7 R  773 c   OFFSET PST_7
0138 01C7 R  774 c   OFFSET PST_8
013A 01C8 R  775 c   OFFSET PST_9
013C 01DD R  776 c   OFFSET PST_A
013E 01F1 R  777 c   OFFSET PST_B
0140 0204 R  778 c   OFFSET PST_COUT
0142 0204 R  779 c   OFFSET PST_COUT
0144 0204 R  780 c   OFFSET PST_DOUT
0146 0204 R  781 c   OFFSET PST_DOUT
0148 026 0410 R CF 782 c   ENV_X PROC NEAR ; SET 4x25 COLOR ALPHA
014B 026 0410 R CF 783 c   AND EQUIP_LOW,0CFH
014D 026 0410 R 10 784 c   OR EQUIP_LOW,010H
0155 CD 10     785 c   MOV AX,01H
0157 C3        786 c   INT 10H
0159 C3        787 c   RET
015B CD 10     788 c   ENV_X ENDP
015D CD 10     789 c   ENV_O PROC NEAR ; SET 80x25 COLOR ALPHA
015F 026 0410 R CF 790 c   AND EQUIP_LOW,0CFH
0161 026 0410 R 20 791 c   OR EQUIP_LOW,020H
0165 CD 10     792 c   MOV AX,03H
0167 C3        793 c   INT 10H
0169 CD 10     794 c   ENV_O ENDP
016B CD 10     795 c   ENV_3 PROC NEAR ; SET MONOCHROME ALPHA
016D 026 0410 R 30 796 c   OR EQUIP_LOW,030H
0171 CD 10     797 c   MOV AX,07H
0173 C3        798 c   INT 10H
0175 CD 10     799 c   ENV_3 ENDP
0177 CD 10     800 c   PST_0: AND INFO,AH
0179 E8 0168 R 801 c   CALL ENV_3
017D C3        802 c   RET
017F CD 10     803 c   PST_1: AND INFO,AH
0181 E8 0168 R 804 c   CALL ENV_3
0185 C3        805 c   RET
0187 CD 10     806 c   PST_2: AND INFO,AH
0189 E8 0168 R 807 c   CALL ENV_3
018D C3        808 c   RET
018F CD 10     809 c   PST_3: AND INFO,AH
0191 E8 0168 R 810 c   CALL ENV_3
0195 C3        811 c   RET
0197 CD 10     812 c   PST_4: MOV DH,3
0199 E8 0168 R 813 c   CALL AL,0
019D C3        814 c   OUT DX,AL
019F CD 10     815 c   OR INFO,AH
01A3 E8 0168 R 816 c   CALL ENV_X
01A7 C3        817 c   RET
01A9 CD 10     818 c   PST_5: MOV DH,3
01AB E8 0168 R 819 c   CALL AL,0
01AF C3        820 c   OUT DX,AL
01B1 CD 10     821 c   OR INFO,AH
01B5 E8 0168 R 822 c   CALL ENV_X
01B9 C3        823 c   RET
01BB CD 10     824 c   PST_6: AND INFO,AH
01BD E8 0168 R 825 c   CALL ENV_3
01BF C3        826 c   CALL ENV_0
01C1 CD 10     827 c   PST_7: AND INFO,AH
01C3 E8 0168 R 828 c   CALL ENV_3
01C7 C3        829 c   CALL ENV_0
01C9 CD 10     830 c   PST_8: AND INFO,AH
01CB E8 0168 R 831 c   CALL ENV_3
01CD C3        832 c   CALL ENV_0
01CE CD 10     833 c   PST_9: AND INFO,AH
01CF E8 0168 R 834 c   CALL ENV_3
01D1 C3        835 c   CALL ENV_0
01D3 CD 10     836 c   PST_A: MOV DH,4
01D5 E8 0168 R 837 c   CALL AL,0
01D9 C3        838 c   OUT DX,AL
01DB CD 10     839 c   OR INFO,AH
01DD E8 0168 R 840 c   CALL ENV_X
01DF C3        841 c   RET
01E1 CD 10     842 c   PST_B: MOV DH,4
01E3 E8 0168 R 843 c   CALL AL,0
01E7 C3        844 c   OUT DX,AL
01E9 CD 10     845 c   OR INFO,AH
01EB E8 0168 R 846 c   CALL ENV_X
01EF C3        847 c   RET
01F1 CD 10     848 c   PST_C: MOV DH,4
01F3 E8 0168 R 849 c   CALL AL,0
01F7 C3        850 c   OUT DX,AL
01F9 CD 10     851 c   OR INFO,AH
01FB E8 0168 R 852 c   CALL ENV_X
01FD C3        853 c   CALL ENV_3
0201 CD 10     854 c   PST_D: MOV DH,4
0203 E8 0168 R 855 c   CALL AL,0
0207 C3        856 c   OUT DX,AL
0209 CD 10     857 c   OR INFO,AH
020D E8 0168 R 858 c   CALL ENV_X
020F C3        859 c   CALL ENV_3
0211 CD 10     860 c   PST_E: MOV DH,4
0213 E8 0168 R 861 c   CALL AL,0
0217 C3        862 c   OUT DX,AL
0219 CD 10     863 c   OR INFO,AH
021D E8 0168 R 864 c   CALL ENV_X
021F C3        865 c   CALL ENV_3
0221 CD 10     866 c   PST_F: MOV DH,4
0223 E8 0168 R 867 c   CALL AL,0
0227 C3        868 c   OUT DX,AL
0229 CD 10     869 c   OR INFO,AH
022D E8 0168 R 870 c   CALL ENV_X
022F C3        871 c   CALL ENV_3
0231 CD 10     872 c   PST_G: MOV DH,4
0233 E8 0168 R 873 c   CALL AL,0
0237 C3        874 c   OUT DX,AL
0239 CD 10     875 c   OR INFO,AH
023D E8 0168 R 876 c   CALL ENV_X
023F C3        877 c   CALL ENV_3
0241 CD 10     878 c   PST_H: MOV DH,4
0243 E8 0168 R 879 c   CALL AL,0
0247 C3        880 c   OUT DX,AL
0249 CD 10     881 c   OR INFO,AH
024D E8 0168 R 882 c   CALL ENV_X
024F C3        883 c   MK_ENV ENDP
THIS ROUTINE TESTS THE CRT CARD INTERNAL DATA BUS AND IN A LIMITED WAY TESTS THE CRTC VIDEO CHIP BY WRITING/READING FROM CURSOR REGISTER. CARRY IS SET IF AN ERROR IS FOUND

REGISTERS BX, SI, ES, DS ARE PRESERVED.

REGISTER AX, DI ARE MODIFIED.

CD_PRESENCE_TST PROC NEAR

    MOV BX, 07FH ; SAVE BX
    MOV DI, BX ; INITIAL WORD PATTERN BYTE
    PUSH AX ; SAVE PORT ADDRESS
    CALL RD_CURSOR ; SAVE ORIGINAL VALUE
    MOV SI, AX ; RECOVER PORT ADDRESS
    POP AX ; WRITE CURSOR
    CALL RD_CURSOR ; RECOVER PORT ADDRESS
    PUSH AX ; READ BACK ADDRESS
    CALL RD_CURSOR ; SAME?
    POP AX
    JNZ NOT_PRESENT ; EXIT IF NOT EQUAL
    JMP TST_EX

NOT_PRESENT:
    XOR AX, AX ; SET NOT PRESENT
    POP BX
    RET

TST_EX:
    MOV AX, 1 ; SET PRESENT ON EXIT
    POP BX ; RESTORE BX
    RET

CD_PRESENCE_TST ENDP

MODULE NAME RD_CURSOR
READ CURSOR POSITION [ADDRESS] (FROM CRTC) TO AX
REGISTER AX IS MODIFIED.

RD_CURSOR PROC NEAR
    PUSH AX ; SAVE REGS USED
    MOV DX, AX ; SAVE DX,AX
    MOV AL,C_CSRSL_LOC_HGH ; OUT DX,AL
    INC DX ; IN AL,DX ; RETURN WITH CURSOR POS IN AX
    POP DX ; RESTORE REGS USED
    RET

RD_CURSOR ENDP

MODULE NAME WR_CURSOR
WRITE CURSOR POSITION [ADDRESS] (TO CRTC) WITH CONTENTS OF AX
ALL REGISTERS PRESERVED.

WR_CURSOR PROC NEAR
    PUSH AX ; SAVE REGS USED
    PUSH DX ; PUSH DX
    MOV DX, AX ; MOV DX,AX
    MOV AH,C_CSRSL_LOC_HGH ; CURSOR LOCATION HIGH INDEX
    MOV AL,0FFH ; TEST VALUE
    CALL OUT_DX ; RETURN WITH CURSOR POS IN AX
    POP DX ; RESTORE REGS USED
    POP AX ; RESTORE REGS USED
    RET

WR_CURSOR ENDP

POST:
    ASSUME DS:ABS0,ES:ABS0
    TEST INFO_2 ; CHECK FOR MONOCHROME CARD
    JNC COLOR_PRESENCE_TST ; COLOR CARD INSTALLED
    MOV AX,03B4h ; COLOR,PRESENCE_TST:
    CALL CD_PRESENCE_TST ; COLOR CARD INSTALLED
    CMP AX,1 ; COLOR CARD INSTALLED?
    JE CONT1 ; YES, GO TO COLOR STG
    JMP POD14 ; NO, GO TO MONOCHROME STG

CONT1:
    MOV AH,10H ; MONOCHROME CARD INSTALLED
    JMST OVER ; RESAVE VALUE
    PUSH AX ; RESAVE VALUE
    MOV DX,08000H ; MODE CONTROL B/W CD
    MOV AL,03B6h ; SET MODE FOR B/W CD
    OUT DX,AL ; SET MODE FOR B/W CD
    MOV AL,1 ; YES, GO TO COLOR STG?
    JE E9 ; YES, GO TO COLOR STG
    MOV BH,08h ; BEG VIDEO RAM ADDR COLOR CD
    MOV CH,04h ; SET MODE TO 0 FOR COLOR CD
    DEC AL ; SET MODE TO 0 FOR COLOR CD
    JE E9 ; DISABLE VIDEO FOR COLOR CD
    MOV BP,DS:RESET_FLAG ; POD INITIALIZED BY KBD RESET?
    POP AX ; POINT ES TO VIDEO RAM STG
0294 7c 07
0296 b9 0d 0b
0298 e8 02df r
029b 75 2e
1009 c je e10 ; YES - SKIP VIDEO RAM TEST
1010 d mov bx,dx ; POINT DS TO RAM STG
1012 c assume ds:nothing, es:nothing
1013 c call stgtst_ont
1014 c je e17 ; GO TEST VIDEO R/W STG
1015 c r/w stg failure - beep spk
1016 c
1017 c setup video data on screen for video line test
1018 c enable video signal and set mode.
1019 c display a horizontal bar on screen.
1020 c
1021 e10: pop ax
1022 c push ax ; GET VIDEO SENSE SMS (AH)
1023 c save it
1024 c ax,ax,7020h ; WRT BLANKS IN REVERSE VIDEO
1025 c sub bx,di ; SETUP START/STOP LOG
1026 c mov cx,40 ; NO OF BLANKS TO DISPLAY
1027 c rep stosw ; WRITE VIDEO STORAGE
1028 c
1029 c crt interface lines test
1030 c
1031 c desc: sense on/off transition of the video enable
1032 c and horizontal sync lines.
1033 c
1034 c pop ax ; GET VIDEO SENSE SM INFO
1035 c push ax ; SAVE IT
1036 c ah,ah,30h ; READ CARD ATTACHED?
1037 c mov dx,03a0h ; SETUP ADDR OF BM STATUS PORT
1038 c int 15h ; YES - TEST LINE 1
1039 c je e11 ; COLOR CARD IS ATTACHED
1040 c mov dl,0dah ; LINE_1ST:
1041 c line_1st:
1042 c e11: mov ah,8 ; OFLOOP:CNT
1043 c
1044 c e12: sub cx,cx ; READ CRT STATUS PORT
1045 c in al,dx ; CHECK VIDEO/HORIZ LINE
1046 c and al,ah ; ITS ON - CHECK IF IT GOES OFF
1047 c jnz e14 ; LOOP TILL OR ON TIMEOUT
1048 c loop e12 ; LOOP TILL IT DOES GO
1049 c jmp short e17 ; CRT_ERR
1050 c
1051 c e14: sub cx,cx ; READ CRT STATUS PORT
1052 c in al,dx ; CHECK VIDEO/HORIZ LINE
1053 c and al,ah ; ITS ON - CHECK NEXT LINE
1054 c jz e15 ; LOOP TILL IT DOES GO
1055 c jmp short e17 ; CRT_ERR
1056 c
1057 c e15: mov dx,102h ; GO BEEP SPEAKER
1058 c call pre_bEEP ; SHORT E18
1059 c
1060 c e16: mov cl,3 ; NEXT LINE BIT TO CHECK
1061 c shr ah,cl ; GO CHECK HORIZONTAL LINE
1062 c jr e12 ; DISPLAY CURSOR:
1063 c
1064 c e17: pop ax ; GET VIDEO SENSE SMS (AH)
1065 c short p0014 ; GET NEXT BIT TO CHECK
1066 c
1067 c this subroutine performs a read/write storage test on
1068 c a block of storage.
1069 c entry:
1070 c p0014: es = address of storage segment being tested
1071 c di = address of storage segment being tested
1072 c when entering at stgtst_ont, cx must be loaded with
1073 c the byte count.
1074 c exit:
1075 c zero flag = 0 if storage error (data compare or parity check).
1076 c al = 0 denotes a parity check, else al=0x00'ed bit
1077 c actual data read.
1078 c ax,bx,cx,dx,di, and si are all destroyed.
1079 c
1080 c stgtst proc near
1081 c stgtst_ont proc near, cx,4000h ; SETUP CNT TO TEST A 16K BLK
1082 c
1083 c stgtst_cnt:
1084 c cli ; SET DIR FLAG TO INCREMENT
1085 c mov bx,cx ; SAVE CNT (4K FOR VIDEO OR 16K)
1086 c mov ax,0aaaah ; GET DATA PATTERN TO WRITE
1087 c mov dx,0a0f5h ; SETUP DATA PATTERN USING TO USE
1088 c sub di,di ; DI = OFFSET RELATIVE TO ES REG
1089 c rep stosb ; WRITE STORAGE LOCATIONS
1090 c stgoto ; POINT TO LAST BYTE JUST WRITTEN
1091 c set dir flag to go backwards
1092 c
1093 c stgtst_cnt:
1094 c dec di ; SETUP BYTE CNT
1095 c mov si,di ; INNER TEST LOOP
1096 c xor al,al ; READ AND TEST BYTE [SI]
1097 c jne ct ; DATA READ AS EXPECTED?
1098 c mov al,dl ; NO - GO TO ERROR ROUTINE
1099 c mov dl,al ; GET NEXT DATA PATTERN TO WRITE
1100 c stosb ; WRITE INTO LOCATION JUST READ
1101 c loop stgtst_cnt ; DECREMENT COUNT AND LOOP CX
1102 c
1103 c ending o pattern written to stc? ; ENDING O PATTERN WRITTEN TO STC?
1104 c je stgoto ; YES - GO TO NEXT LOCATION, AL=0
1105 c mov ah,al ; SETUP NEW VALUE FOR COMPARE
1106 c xchg dh,dl ; MOVE NEXT DATA PATTERN TO DL
1107 c ret ; CONTINUE TEST SEQUENCE TILL O
1108 c
1109 c ct: inc di ; CONTINUE TEST SEQUENCE TILL O
1110 c jnz stgoto ; ELSE, GO FOR END RESULT PATTERN
1111 c
1112 c and al,ah ; MAKE TIME FORWARD PASS
1113 c
1114 c cg6: cld ; SET DIR FLAG TO GO FORWARD
1115 c set pointer to beg location
1116 c inc di ; READ/WRITE FORWARD IN STG
1117 c jz ct ; ADJUST POINT
1118 c dec di ; READ/WRITE BACKWARD IN STG
1119 c
1120 c cg6x: mov al,000h ; AL=0 DATA COMPARE OK
1121 c
1122 c ct: ret ; SET DIRECTION FLAG BACK TO INC
1123 c
1124 c stgtst endp
1125 c
1126 c ega crt attachment test
1127 c
1128 c 1. init crt to hmx25 - bv ****set to mode****
1129 c 2. check for vertical and video enables, and check
1130 c timing of same
1131 c 3. check for interrupt
1132 c 4. check red, blue, green, and intensify dots
; 5. INIT TO 40X25 - COLOR/MONO ****SET TO MODE**** :
;-------------------------------------------------------------
;---- NOMINAL TIME IS 826GH FOR 60 Hz.
;---- NOMINAL TIME IS 426GH FOR 30 Hz.
;-------------------------------------------------------------
= A04C
= C460
= 00C8
= B099
= 8662
= 015E
= 015E
= 0043
= 0040
= 0317
= 031A 83 EC 0A
= 031A 88 EC
031C E8 0CFC R
031F BO 30
0323 E6 43
0325 E6 40
0325 E6 00 0487 R 02
032C 7E 1F
032E E8 0164 R
0332 E8 0162 015E
0336 C7 46 04 D059
0336 C7 46 06 8662
0336 C7 46 06 8662
0342 B4 01
0342 B4 07
0346 E8 0015 R
0346 B2 0A
034D E8 01A8 R
0350 E8 0E9A R
0353 73 11
0357 84 01
0357 84 01
035B 00 0015 R
035C 73 46 02 015E
0360 E8 0590
0366 C7 46 02 00C8
0368 86 06 ACAC
0370 C7 46 06 C460
0375 DA 2B
0377 03 00 0500
037A 80 C9
037C 2B C9
037E EC
0381 75 07
0383 E2 F9
0387 E9 0448 R
038A B6 00
038C E6 40
038E 2B DB
0390 33 C9
0392 EC
0393 A8 08
0395 00
0397 E2 F9
0399 B3 01
039B E9 0448 R
039E 2B C9
03A0 EC
03A3 74 15
03A7 48 23
03A9 E2 F2
03A9 E2 F2
03AD E9 0448 R
03B0 00
03B3 B3 03
03B2 E9 0448 R
03B5 B3 04
03B7 E9 0448 R
03BA 86 08
03BE 75 F2
03BF EC
03C1 E8 01
03C1 E8 01
03C3 E8 01
1135
1136
1137
1138
1139
1140
1141
1142
1143
1144
1145
1146
1147
1148
1149
1150
1151
1152
1153
1154
1155
1156
1157
1158
1159
1160
1161
1162
1163
1164
1165
1166
1167
1168
1169
1170
1171
1172
1173
1174
1175
1176
1177
1178
1179
1180
1181
1182
1183
1184
1185
1186
1187
1188
1189
1190
1191
1192
1193
1194
1195
1196
1197
1198
1199
1200
1201
1202
1203
1204
1205
1206
1207
1208
1209
1210
1211
1212
1213
1214
1215
1216
1217
1218
1219
1220
1221
1222
1223
1224
1225
1226
1227
1228
1229
1230
1231
1232
1233
1234
1235
1236
1237
1238
1239
1240
1241
1242
1243
1244
1245
1246
1247
1248
1249
1250
1251
1252
1253
1254
1255
1256
1257
1258
1259
1260
MAX_VERT_COLOR EQU 040ACH ; MAX TIME FOR VERT/VERT (NOMINAL * 10%)
MIN_VERT_COLOR EQU OC640H ; MIN TIME FOR VERT/VERT (NOMINAL * 10%)
CENAB_PER_FRAME EQU 200 ; NUM OF ENABLES PER FRAME
MAX_VERT_MONO EQU 0BD99H ; MAX TIME FOR VERT/VERT (NOMINAL * 10%)
MIN_VERT_MONO EQU 0B624H ; MIN TIME FOR VERT/VERT (NOMINAL * 10%)
EENAB_PER_FRAME EQU 350 ; ENHANCED ENABLES PER FRAME
MENAB_PER_FRAME EQU 350 ; NUM OF ENABLES PER FRAME
TIN_CTL EQU 043H ; 8253 TIMER CONTROL PORT
TIMERO EQU 040H ; 8253 TIMER/COUNTER 0 PORT
POD14 PROC NEAR
SUB SP, 5 ; RESERVE 5 WORDS ON STACK
MOV BP, SP ; INIT SCRATCH POINT
ASSUME DS:ABS0, ES:ABS0
CALL DDS
MOV AL, 00110000B ; SET TIMER 0 TO MODE 0
OUT TIN_CTL, AL
MOV AH, 01
OUT TIMERO, AH ; SEND FIRST BYTE TO TIMER
JZ COLOR_EGA_V
CALL EMV_X
MOV WORD PTR[BP+12],MENAB_PER_FRAME ; SET UP IN MONOCHROME
MOV WORD PTR[BP+14],MAX_VERT_MONO ; SET UP IN MONOCHROME
MOV WORD PTR[BP+16],MIN_VERT_MONO ; SET UP IN MONOCHROME
MOV AH, RTC_ADDR ; HORIZ. TOTAL DISPLAY
CALL OUT_DX
MOV DL, INPUT_STATUS_B ; TO 40 COL. TOTAL
JMP SHORT COMMON
COLOR_EGA_V:
CALL BRST_DET
JNC COLOR_Y
MOV AH, 01 ; BRST MODE ONLY!
CALL OUT_DX
MOD WORD PTR[BP+12],FENAB_PER_FRAME ; NUM. OF FRAMES FOR COLOR
JMP BRST_COLOR_Y
BRST_COLOR_Y:
MOV WORD PTR[BP+12],CENAB_PER_FRAME ; NUM. OF FRAMES FOR COLOR
MOV WORD PTR[BP+14],MAX_VERT_COLOR ; MAX TIME FOR VERT/VERT
MOV WORD PTR[BP+16],MIN_VERT_COLOR ; MIN TIME FOR VERT/VERT
MOV DL, INPUT_STATUS ; SET ADDRESSING TO VIDEO
JMP SHORT COMMON
COMMON:
MOV AX, 0500H ; SET TO VIDEO PAGE 0
INT 10H
SUB CX, CX
;----- LOOK FOR VERTICAL
POD14_1:
IN AL, DX ; GET STATUS
JNE POD14_2 ; VERTICAL THERE YET?
CONTINUE IF IT IS
LOOP POD14_1 ; KEEP LOOKING TILL COUNT
MOV BL, 01H ; EXHAUSTED
JMP POD14_ERR ; NO VERTICAL
;----- GOT VERTICAL - START TIMER
POD14_2:
OUT TIN_CTL, AL ; SEND 2ND BYTE TO TIMER TO START IT
BX BX ; INIT. ENABLE COUNTER
IN AL, DX ; GET STATUS
JZ POD14_2 ; VERTICAL STILL THERE
JMP POD14_3 ; KEEP LOOKING TILL COUNT
;----- WAIT FOR VERTICAL TO GO AWAY
POD14_25:
IN AL, DX ; GET STATUS
JE POD14_4 ; VERTICAL STILL THERE
JZ POD14_25 ; KEEP LOOKING TILL COUNT
JMP POD14_ERR ; VERTICAL STUCK ON
;----- NOW START LOOKING FOR ENABLE TRANSITIONS
POD14_3:
SUB CX, CX
POD14_4:
IN AL, DX ; GET STATUS
TEST AL, 00000001B ; ENABLED YET?
JE POD14_5 ; GO ON IF IT IS
POD14_6:
IN AL, DX ; VERTICAL ON AGAIN?
JZ POD14_7 ; KEEP LOOKING IF NOT
LOOP POD14_6 ; KEEP LOOKING IF NOT
JMP POD14_ERR ; ENABLE STUCK OFF
POD14_5:
MOV BL, 03H ; VERTICAL STUCK ON
JMP POD14_ERR ; ENABLE STUCK ON
;----- MAKE SURE VERTICAL WENT OFF WITH ENABLE GOING ON
POD14_5:
TEST AL, 00001000B ; VERTICAL OFF?
JNZ POD14_6A ; GO ON IF IT IS
;----- NOW WAIT FOR ENABLE TO Go OFF
POD14_6:
IN AL, DX ; GET STATUS
TEST AL, 00000001B ; ENABLED YET?
JZ POD14_6B ; KEEP LOOKING IF NOT
JMP POD14_6B ; KEEP LOOKING IF NOT
;----- ENABLE HAS TOGGLED, BUMP COUNTER AND TEST FOR NEXT VERTICAL
03C5
03C6 43
03C6 74 04
03CA 8B 08
03CA 74 D2
03CC
03CC B0 00
03CE E6 42
03D0 8B 02
03D1 74 08
03D5 B3 05
03D7 E8 6F
03D9 E4 46
03DB E4 E0
03DE E4 90
03E3 90
03E4 90
03E5 90 46 04
03E7 70 04
03E9 70 04
03EB EB 58
03ED 90
03EF 38 46 05
03F0 7E 04
03F2 7E 07
03F4 EB 3C
03F6 BB 0908
03F7 BB 000F
03FC 89 0050
03FF EC
0401 EC
0403 90
0404 84 C0
0405 84 CF
0407 84 0F
0409 8B 0115 R
040B 84 000F
040D 8A 00
040F 8A 00
0410 8A 00
0411 8A 00
0412 8A 00
0413 8A 00
0414 8A 00
0415 8A 00
0416 8A 00
0417 8A 00
0418 2B C9
041D EC
041E 75 30
0420 75 09
0422 75 19
0424 83 36
0426 83 DC
0428 EB 90
042B 8B C9
042D EC
042E A8 30
0430 74 08
0432 EB F9
0434 83 20
0436 83 00
0438 EB OE
043A FE C8
043C 74 30
043F 74 25
0441 80 CC 0A
0444 80 C8
0446 EB C8
0448 89 0006
044B BA 0103
044E 8B 0028 R
0451 83 C4 0A
0454 80 36
0456 83 48
0458 2A C0
045A 80 40
045C 90
045D 80 40
0460 BD 0001
0463 E9 0091 R
0466 89 0006
0469 8B 0500
046C CD 10
046E 8B 00
0470 E6 43
0472 8B C0
0474 E6 40
0476 90
0477 8B 40
0479 8B CA
047A BD 0000
0480 1E
0480 1E
0480 1E

P0D14_7:
    INC BX
    JZ P0D14_75
    TEST AL,00001000B
    JZ P0D14_3
    ; IF COUNTER WRAPS, MACHINE IS WRONG
    ; DID ENABLE COUNT?
    ; BECAUSE OF VERTICAL CYCLE NOT TO LOOK FOR ANOTHER
    ; ENABLE COUNT
    ; HAVE COMPLETE VERTICAL-VERTICAL CYCLE, NOW TEST RESULTS
P0D14_75:
    MOV AL,00
    OUT T1M_CTL,AL
    CMP DX,NORD PTR[BP][2]
    ; NUMBER OF ENABLES BETWEEN VERTICALS OK?
    ; GET TIMER VALUE LOW
    IN AL,TIMER0
    MOV AH,AL
    ; SAVE IT
    IN AL,TIMER0
    ; GET TIMER HIGH
    CMP AX,WORD PTR[BP][4]
    ; MAXIMUM VERTICAL TIMING
    JGE P0D14_9
    MOV P0D14_9,AX
    SHLD P0D14_ERR
    ; MINIMUM VERTICAL TIMING
    CMP AX,WORD PTR[BP][16]
    JLE P0D14_10
    MOV P0D14_10,AX
    SHLD P0D14_ERR
    ; SEE IF RED, GREEN, BLUE AND INTENSIFY DOTS WORK
P0D14_13:
    ; FIRST, SET A LINE OF REVERSE VIDEO, INTENSIFIED BLANKS INTO BUFFER
    MOV AX,090BH
    MOV BX,000H
    ; WRITE CHARS, BLANKS
    PUSH DS
    ; PAGE 0, ALL ON VIDEO,
    ; HIGH INTENSITY
    ; 80 CHARACTERS
    INT 80
    IN AL,DX
    ; SAVE INPUT STATUS
    PUSH DX
    ; ATTRIBUTE ADDRESS
    MOV AH,0FH
    INT 10
    ; PALETTE REG 'F'
    CALL OUT_X
    ; VIDEO STATUS MUX
    MOV AX,0FH
    ; START WITH BLUE DOTS
    POP DX
    POP DS
    ; SAVE INPUT STATUS
    PUSH DX
    ; ATTRIBUTE ADDRESS
    MOV DL,ATTR_WRITE
    ; CLEAR ALL ENABLE
    CALL OUT_X
    ; VIDEO STATUS MUX
    POP DX
    ; RECOVER INPUT STATUS
    SUB CX,CX
    ; SEE IF DOT COMES ON
P0D14_14:
    IN AL,DX
    ; GET STATUS
    TEST AL,00110000B
    ; DOT THERE?
    JNZ P0D14_15
    LOOP P0D14_14
    ; CONTINUE TEST FOR DOT ON
    OR BL,AL
    ; OR IN DOT BEING TESTED
    ; DOT NOT COMING ON
P0D14_15:
    SUB CX,CX
    ; SEE IF DOT GOES OFF
P0D14_16:
    IN AL,DX
    ; GET STATUS
    TEST AL,00110000B
    ; IS DOT STILL ON?
    JE P0D14_17
    LOOP P0D14_16
    ; ELSE KEEP WAITING FOR DOT TO GO OFF
    OR BL,AL
    ; OR IN DOT BEING TESTED
    ; ADJUST TO POINT TO NEXT DOT
P0D14_17:
    INC AH
    CMP AH,001DH
    ; ALL 3 DOTS DONE?
    JG P0D14_18
    ; SO END
    OR AH,0FH
    ; MAKE OF,1F,2F
    JMP P0D14_13
    ; GO LOOK FOR ANOTHER DOT
    ; ONE LONG AND THREE SHORT
P0D14_18:
    CALL DDS
    MOV AX,0500H
    ; SET TO VIDEO PAGE 0
    INT 10H
    ; RE-INIT TIMER 0
    OUT T1M_CTL,AL
    SUB AL,AL
    ; BALANCE STACK
    OUT T1M_CTL,AL
    ; RE-INIT TIMER 0
    ; REMOVE SCRATCH PAD
    ADD SP,3AH
    ; MAKE BP NON ZERO
P0D14_19:
    ; TEST STORAGE
    MEM_TEST:
    PUSH DS
049E  E8 C0FE R    1367 CALL DDS
        1368 ASSUME DS:.ABSO
04A0  F6 06 0B47 R 02 1369 TEST INFO_2
        1370 JZ .EQU1
04A2  84 C0 50     1371 OR EQUIP_LOW,030H
04A5  BB 0F 080    1372 MOV AX,.0FR
04A8  0F 27       1373 OR INT,07DH
        1374 MOV AX,.0FH
04AA  EB D0       1375 JMP SHORT D_OUT_M
04AB  8D 4C 04 1   1376 b_COLOR_M:
04AF  80 26 0410 R CF 1377 AND EQUIP_LOW,,OCFH
04B2  BB 00        1378 MOV DL,#020H
04B4  FE           1379 MOV AL,#040H
04B5  A1 0400      1380 DOS assuming the nothing,ES:NOTHING
04B8  8D 4C 04 1   1381 MOV DS,AX
        1382 MOV WORD PTR [BP+j],0
04BB  8D 5C 04 1   1383 MOV WORD PTR [BP+14],0
04BE  80 02        1384 MOV DH,3
04C0  69 24 03     1385 MOV DX,ADDR
04C3  8B 08        1386 MOV AX,A200H
04C5  8B 44 04 1   1387 call OUT_DP
04CA  88 35        1388 MOV AL,S200H_ADDR
04C2  8B FF        1389 MOV AH,000H
04C4  89 58        1390 call OUT_DX
04C6  E9 00 00     1391 PUSH BX
        1392 MOV DL,DL_ATTR_READ
04C9  AA            1393 mov dl,0
04CA  BD 02 00     1394 JD (eg mem err )
        1395 call memory_ok
04CF  8B 5A         1396 cmp ah,0
04D1  8B 46 0E     1397 JZ AA2
04D4  8D 5E 04      1398 jmp ega_mem_error
04D7  8D 4C 04 1   1399 AA2: POP DX
        1400 MOV DX, SEQ_ADDR
04DA  8B 56        1401 MOV AX,A100H
04DB  89 54 02 00  1402 call OUT_DX
04DE  C3            1403 ret
; internal color mode
; test in color
; reserve 3 words on stack
; set bp
; put buffer address in ax
; set up dec regs to point
; to buffer area
; initialize
; initialize
; internal read map select
; address read map select
; set up attribute
; attribute write address
; address of read map
; set up attribute
; attribute write address
; go find amount of memory
; go test it
; go test it
; go test it
; ADDRESS Of READ MAP
; ADDRESS Of READ MAP
; SET UP ATTRIBUTE
; ATTRIBUTE WRITE ADDRESS
; INITIALIZE
; GO FIND AMOUNT Of MEMORY
; ADDRESS OF READ MAP
; ADDRESS OF READ MAP
; SET UP ATTRIBUTE
; ATTRIBUTE WRITE ADDRESS
; INITIALIZE
; GO FIND AMOUNT Of MEMORY
; GO TEST IT
;GO TEST IT
;GO TEST IT
;SAVE SCRATCH PAD POINTER
;RESTORE
;IMPORTANT
;Restorand segment
05AC D3 E3
05A1 80 60 E0 60
05B1 80 26 0487 R 9F
05B6 0E 1E 0487 R
05BA 60 0E 0487 R 04
05BF 8A 1E 0488 R
05C2 88 0013 R
05C5 88 C4 00
05C8 1F
05CD 09 0091 R
05CE BA 0103
05D0 09 0658 R
05D3 55 0001
05D6 BD 0001
05D9 EB C3
05E0 15 09
05E3 89 AD00
05E6 8E DB
05E9 88 45 04
05EC 8A 88
05EF 88 01
05F1 E1 01
05F4 88 05FB R
05F7 88 0000
05FA C3
05FB

05F9 88 46 04
05FC 01 56 02
05FE 88 0000
05FF C3
0600

0601 E8 OCFC R
0604 88 8E 072 R
0607 88 1234
060A 8C 82
060D 8A DA
0610 74 82
0613 74 4321
0616 74 5C
0619 88 05
061C 8A 03
061F 32 C4
0622 FE C4
0625 FE C4
0628 75 F2
062B BB E9
062E BB AA55
0631 BB DD
0634 BB 5AA5
0637 F3/ A8
063A 4F
063D 4F
0640 7D
0643 88 F7
0646 88 CD
0649 AD
064C 88 C3
064F 79 22
0652 88 CB
0655 AB
0658 E2 F6
065B 88 89
065E 46
0661 88 FE
0664 AD
0667 33 C2
066A 88 11
066D 88 9
0670 E2 F8
0673 FD
0676 NE
0679 4E
067C 88 CD
067F AD
0682 88 CO
0685 E2 F5
0688 E2 F9
068B EB 11
068E 16
0691 16
0694 BB C8
0697 BB 32
069A 0A ED

1513 SHL BX,CL
1514 AND BL,01100000B ; ISOLATE BITS 5 AND 6
1515 AND INFO,1001111118
1516 OR INFO,BL
1517 OR INFO,00000100B ; OUR SET 3XX ACTIVE
1518 MOV BL,INFO_3
1519 CALL MK_ENV
1520 ADD SP,76 ; RESTORE STACK
1521 POP DS
1522 POP BP ; GO TO END
1523 JMP EGA_MEM_ERR:
1524 MOV DX,0103H ; ONE LONG AND THREE SHORT
1525 CALL DEL_BEL
1526 PUSH BP ; SAVE SCRATCH PAD POINTER
1527 PUSH BP ; INDICATE ERROR FOR XT
1528 JM ESA_MEM_EXIT

; THIS ROUTINE FINDS AMOUNT OF MEMORY GOOD

MEMORY_OK PROC NEAR
1535 MOV DX,0A000H ; SET PTR. TO BUFFER SEG
1536 MOV DS,BX ; SET SEG. REG.
1537 MOV ES,DS ; SET COUNT FOR 32K WORDS
1538 MOV WORD PTR[BP][14],0 ; SET AMOUNT OF BUFFER
1539 MOV CX,0 ; TO 0
1540 SUB CL,CL ; MULTIPLY BY TWG
1541 CALL PODSTG
1542 PUSH AX ; TEST FOR ERROR
1543 INZ MEMORY_OK_ERR ; IF ERROR GO PRINT IT
1544 MOV AX,WORD PTR[BP][14] ; AMOUNT OF MEMORY FOUND
1545 ADD WORD PTR[BP][2],AX ; AMOUNT OF MEMORY GOOD
1546 RET
1547 ENDP

MEMORY_OK_ERR:
1548 INZ MEMORY_OK_ERR ; IF ERROR GO PRINT IT
1549 RET
1550 ENDP

; THIS ROUTINE PERFORMS A READ/WRITE TEST ON A BLOCK OF STORAGE
; (BLOCK SIZE = 32KW). IF "WARM START", FILL BLOCK WITH 0000 AND
; RETURN.
; ON ENTRY:
; DS = ADDRESS OF STORAGE TO BE TESTED
; ES = ADDRESS OF STORAGE TO BE TESTED
; CX = WORD COUNT OF STORAGE BLOCK TO BE TESTED
; DS:DX = SECTOR NUMBER
; ON EXIT:
; FLAG = OFF IF STORAGE ERROR
; AX,BX,CX,DX,SI ARE ALL DESTROYED.

PODSTG PROC NEAR
1555 PUSH BP
1556 CLD
1557 MOV DI,0 ; SET DIR TO INCREMENT
1558 MOV SI,0 ; SET DI=0000 REL TO START
1559 MOV AX,ES ; SET SEGMENT
1560 MOV AL,0 ; INITIATE PATTERN FOR 00-FF TEST
1561 SUB AX,AX
1562 CALL DOS ; WARM START?
1563 CMP AX,0 ; IF WARM START
1564 JE PODSTG_S ; DCP START?
1565 JNE PODSTG_T ; DO NOT SO
1566 MOV [DI],AL ; WRITE TEST DATA
1567 MOV AL,[DI] ; GET IT BACK
1568 XOR AL,AH ; COMPARE TO EXPECTED
1569 INC [PODSTG_ERR] ; FORM NEW DATA PATTERN
1570 MOV AL,0 ; LOOP TILL ALL 256 DATA
1571 STOSW ; PATTERNS DONE
1572 MOV AX,0A55H ; SAVE CHECK POINT
1573 STOSW ; LOAD DATA PATTERN
1574 MOV AX,0AA5AH ; LOAD OTHER DATA PATTERN
1575 STOSW ; FILL WORDS FROM LOW TO HIGH
1576 DEC DI ; POINT TO LAST WORD WRITTEN
1577 MOV S1,0 ; SET INDEX REGS EQUAL
1578 MOV CX,BP ; REVERSE COUNT
1579 LODSW ; FROM HIGH TO LOW
1580 JNZ AX,BX ; GET WORD FROM MEMORY
1581 MOV AX,0 ; CLEAR WORD THEREIF
1582 STOSW ; ERROR EXIT IF NOT
1583 MOV AX,0 ; GET 512 DATA HERE AND
1584 STOSW ; STORE LOC JUST READ
1585 LOOP PODSTG_2 ; LOOP TILL ALL BYTES DONE
1586 MOV CX,BP ; FORCE BACK TO INCREMENT
1587 DEC SI ; BACK TO INCREMENT
1588 INC SI ; ADJUST PTRS
1589 MOV DI,S1 ; POINT TO HIGH DOING WORDS
1590 LODSW ; GET A WORD
1591 XOR AX,DX ; SHOULD COMPARE TO DX
1592 INC [PODSTG_ERR] ; IF NOT
1593 STOSW ; WRITE 0000 BACK TO LOC
1594 LOOP PODSTG_3 ; LOOP TILL DONE
1595 MOV CX,BP ; BACK TO DECREMENT
1596 DEC SI ; ADJUST POINTER DOWN TO LAST WORD WRITTEN
1597 MOV S1,0 ; GET WORD COUNT
1598 MOV CX,BP ; GET WORD
1599 XOR AH,0 ; SHOULD NOT
1600 LOOP PODSTG_ERR2 ; LOOP TILL DONE

PODSTG_ERR2:
1603 MOV CX,AX ; SAVE BITS IN ERROR
1604 MOV AH,CH ; HIGH BYTE ERROR?
6656 74 02
0668 b4 01
066a 0a c9
066c 74 01
066e 80 c4 02
0671 50
0672 50
0673 c3
0674 1639 c
1640 c
1641 c
1642 c
1643 c
1644 c
1645 c
1646 c
1647 c
1648 c
1649 c
1650 c
1651 c
1652 c
1653 c
1654 c
1655 c
1656 c
1657 c
1658 c
1659 c
1660 c
1661 c
1662 c
1663 c
1664 c
1665 c
1666 c
1667 c
1668 c
1669 NOW_BIG PROC NEAR
1670 MOV DX,ES
1671 SUB DS,ES
1672 FILL_LOOP:
1673 MOV ES,DX
1674 SUB DI,DI
1675 MOV AX,0AA59H
1676 MOV CX,0000H
1677 MOV ES:[DI],AX
1678 MOV AX,ES:[DI]
1679 XOR AX,CX
1680 JZ SHORT_END
1681 MOV CX,2000H
1682 REP STOSW
1683 ADD DX,04000H
1684 CMP DX,16000H
1685 JNZ FILL_LOOP
1686 JMP NOW_BIG_END
1687 JNZ NOW_BIG_END
1688 JMP NOW_BIG_END
1689 NOW_BIG_END:
1690 CMP DH,0A0H
1691 JZ HB_ERROR_EXIT
1692 RESUME:
1693 ADD WORD PTR[BP][4],BX
1694 MOV AX,0
1695 HB_ERROR_EXIT:
1696 RET
1697 NOW_BIG ENDP

;----------------------SUBROUTINE FOR POWER ON DIAGNOSTICS:
1700 ERR_BEEP PROC NEAR
1701 PUSH DS
1702 PUSH DX
1703 CALL DDS
1704 ASSUME DS:ABS0
1705 OR DH,0DH
1706 JZ G1
1707 MOV BL,6
1708 CALL BEEP
1709 G1:
1710 MOV DL,6
1711 CALL BEEP
1712 G2:
1713 LOOP G2
1714 DEC DH
1715 JNZ G2
1716 G3:
1717 MOV BL,1
1718 CALL BEEP
1719 GH:
1720 LOOP GH
1721 DEC DL
1722 JNZ GH
1723 G5:
1724 LOOP G5
1725 POP DS
1726 RESTORE CONTENTS OF DS
1727 RESTORE FLAGS
1728 ERR_BEEP ENDP
1729 SUBTTL
1730 T2 LABEL WORD
1731 DW OFFSET A00
1732 DW OFFSET A01
1733 DW OFFSET A02
1734 DW OFFSET A03
1735 DW OFFSET A04
1736 DW OFFSET A05
1737 DW OFFSET A06
1738 DW OFFSET A07
1739 DW OFFSET A08
1740 DW OFFSET A09
1741 DW OFFSET A0A
1742 DW OFFSET A0B
1743 DW OFFSET A0C
1744 DW OFFSET A0D
1745 DW OFFSET A0E
1746 DW OFFSET A0F
1747 DW OFFSET A10
1748 DW OFFSET A11
1749 DW OFFSET A12
1750 DW OFFSET A13
1751 DW OFFSET A14
1752 DW OFFSET A15
1753 DW OFFSET A16
1754 DW OFFSET A17
1755 DW OFFSET A18
1756 DW OFFSET A19
1757 DW OFFSET A1A
1758 DW OFFSET A1B
1759 DW OFFSET A1C
1760 DW OFFSET A1D
1761 DW OFFSET A1E
1762 DW OFFSET A1F
1763 DW OFFSET A20
1764 T2L EQU $-15

JZ POSTG_ERR1
MOV AH,1 ; SET HIGH BYTE ERROR
OR CL,CL ; LOW BYTE ERROR?
JZ POSTG_ERR2
MOV AH,2
POP BP
RET

POSTG_5:
PUSH AX
PUSH DX
MOV DX,3
MOV DL,SEG ADDR
MOV AX,OFFSET ADDR
CALL OUT_DX
POP DX
REP STOSW
CALL DSABSO
ASSUME DS:RESET_FLAG,BX
MOV DS,RESET_FLAG
JMP POSTG_ERR2 ; AND EXIT

;----------------------DETERMINE SIZE OF BUFFER
POSTG ENDP

;----------------------SUBROUTINE FOR POWER ON DIAGNOSTICS:
ERR_BEEP PROC NEAR
PUSH DS
PUSH DX
CALL DDS
ASSUME DS:ABS0
OR DH,0DH
JZ G1
MOV BL,6
CALL BEEP
G1:
MOV DL,6
CALL BEEP
G2:
LOOP G2
DEC DH
JNZ G2
G3:
MOV BL,1
CALL BEEP
GH:
LOOP GH
DEC DL
JNZ GH
G5:
LOOP G5
POP DS
RESTORE CONTENTS OF DS
RESTORE FLAGS
ERR_BEEP ENDP

;----------------------DETERMINE SIZE OF BUFFER
NOW_BIG PROC NEAR
MOV DX,ES
SUB DS,ES
FILL_LOOP:
MOV ES,DX
SUB DI,DI
MOV AX,0AA59H
MOV CX,0000H
MOV ES:[DI],AX
MOV AX,ES:[DI]
XOR AX,CX
JZ SHORT_END
MOV CX,2000H
REP STOSW
ADD DX,04000H
CMP DX,16000H
JNZ FILL_LOOP
JMP NOW_BIG_END
JNZ NOW_BIG_END
JMP NOW_BIG_END
NOW_BIG_END:
CMP DH,0A0H
JZ HB_ERROR_EXIT
RESUME:
ADD WORD PTR[BP][4],BX
MOV AX,0
HB_ERROR_EXIT:
RET
NOW_BIG ENDP
INCLUDE VPARMS.INC
SUBTTL VPARMS.INC
PAGE
VIDEO_PARMS LABEL BYTE

STRUCTURE OF THIS TABLE

COLUMNS, ROWS, PELS PER CHARACTER
FACE
SEQUENCER PARAMETERS
MISCELLANEOUS REGISTER
CRT PARMS
ATTRIBUTE PARAMETERS
GRAPHICS PARAMETERS

BASE_1 EQU $ - VIDEO_PARMS
BASE_1_L LABEL BYTE

----- DEFAULT MODES -----

;--0--
DB 40D,24D,08D
DW 00800H

TFS_LEN EQU $ - BASE_1_L

SEQ_PARMS LABEL BYTE
M1 DB 00B,003H,009H,003H
M2 EQU $ - SEQ_PARMS
DB 023H

CRT_PARMS LABEL BYTE
DB 037H,027H,02DH,037H,031H,015H
DB 006H,001H,007H,006H,007H,007H
DB 000H,000H,000H,000H,000H,024H
DB 0C7H,014H,080H,0E0H,0F0H,0A3H
DB 01FH

M4 EQU $-CRT_PARMS

LN_4 EQU $ - BASE_1_L

ATTR_PARMS LABEL BYTE
DB 000H,001H,002H,003H,004H,005H
DB 006H,007H,010H,011H,012H,013H
DB 014H,015H,016H,017H,018H,019H
DB 06FH,000H
M5 EQU $-ATTR_PARMS

LN_2 EQU $ - BASE_1_L

GRAPH_PARMS LABEL BYTE
DB 000H,000H,000H,000H,000H,010H
DB 000H,000H,0FFH
M6 EQU $-GRAPH_PARMS

M_TOL_LEN EQU $ - BASE_1_L

;--1--
DB 40D,24D,08D
DW 00800H
DB 008H,003H,000H,033H
DB 023H
DB 037H,027H,02DH,037H,031H,015H
DB 004H,011H,009H,006H,007H,007H
DB 000H,000H,000H,000H,000H,024H
DB 0C7H,014H,080H,0E0H,0F0H,0A3H
DB OFFH

DB 000H,001H,002H,001H,004H,005H
DB 006H,007H,010H,011H,012H,013H
DB 014H,015H,016H,017H,000H,000H
DB 06FH,000H
DB 000H,000H,000H,000H,000H,010H
DB 000H,000H,0FFH

;--2--
DB 80D,24D,08D
DW 01000H
DB 001H,003H,000H,003H
DB 023H.
DB 070H,04FH,05CH,02FH,05FH,007H
DB 004H,011H,000H,007H,006H,007H
DB 000H,000H,000H,000H,000H,024H
DB 0C7H,028H,008H,0E0H,0F0H,0A3H
DB 0FFH
DB 000H,001H,002H,003H,004H,005H
DB 006H,007H,010H,011H,012H,013H
DB 014H,015H,016H,017H,000H,000H
DB 06FH,000H
DB 000H,000H,000H,000H,000H,010H
DB 000H,000H,0FFH

;--3--
DB 80D,24D,08D
DW 01000H
DB 001H,003H,000H,003H
DB 023H.
DB 070H,04FH,05CH,02FH,05FH,007H
DB 004H,011H,000H,007H,006H,007H
DB 000H,000H,000H,000H,000H,024H
DB 0C7H,028H,008H,0E0H,0F0H,0A3H
DB 0FFH
DB 000H,001H,002H,003H,004H,005H
DB 006H,007H,010H,011H,012H,013H
DB 014H,015H,016H,017H,000H,000H
DB 06FH,000H
DB 000H,000H,000H,000H,000H,010H
DB 000H,000H,0FFH

;--4--
DB 40D,24D,08D
August 2, 1984    IBM Enhanced Graphics Adapter 119
098E 00 00 00 00 10 2017 C DB 000H,000H,000H,000H,010H
0994 0E 00 FF 2018 C DB 00EH,000H,0FFH
0995 2020 C DB 000H,000H,000H,000H,010H
0997 28 18 08 2022 C DB 40D,24D,0BD
099A 4000 2024 D DW 04000H
099C 00 00 00 03 2025 C DB 000H,000H,000H,003H
09A0 23 2027 C DB 023H
09A1 37 27 20 31 15 2028 C DB 037H,027H,02DH,037H,031H,015H
09A7 04 11 00 07 06 2030 C DB 004H,011H,000H,007H,006H,007H
09A8 00 00 00 00 E1 24 2031 C DB 000H,000H,000H,000H,0E1H,024H
09B3 14 08 00 00 A3 2032 C DB 0C7H,014H,000H,010H,000H,0A3H
09B9 FF 2033 C DB OFFH
09BA 00 01 02 03 04 05 2034 C DB 000H,001H,002H,003H,004H,005H
09C0 06 07 10 11 12 13 2036 C DB 006H,007H,010H,011H,012H,013H
09C5 00 00 00 00 16 17 2037 C DB 010H,016H,017H,000H,000H
09CC 0F 00 2038 C DB 00FH,000H
09CE 00 00 00 00 10 2040 C DB 000H,000H,000H,000H,000H,010H
09D4 0E 00 FF 2041 C DB 00EH,000H,0FFH
09D7 50 18 08 2043 C DB 80D,24D,0BD
09DA 1000 2045 D DW 01000H
09DC 01 04 00 07 2047 C DB 001H,004H,000H,007H
09E0 23 2049 C DB 023H
09E1 70 4F 5C 2F 5F 07 2051 C DB 070H,04FH,05CH,02FH,05FH,007H
09E5 00 00 00 00 E1 24 2053 C DB 000H,000H,000H,000H,0E1H,024H
09E7 C7 28 08 E0 TO A3 2054 C DB 0C7H,028H,008H,0E0H,0F0H,0A3H
09EF FF 2055 C DB OFFH
09FA 00 00 00 00 00 2057 C DB 000H,000H,000H,000H,000H,000H
0A00 00 00 00 00 00 2059 C DB 000H,000H,000H,000H,000H,000H
0A06 00 00 00 00 00 2059 C DB 000H,000H,000H,000H,000H,000H
0A0C 00 00 00 00 00 2060 C DB 000H,000H,000H,000H,000H,000H
0A0E 00 00 00 00 00 2062 C DB 000H,000H,000H,000H,000H,000H
0A11 00 04 00 FF 2063 C DB 000H,004H,000H,0FFH
0A17 50 18 0E 2065 C DB 80D,24D,14D
0A1A 1000 2066 D DW 01000H
0A1C 00 04 00 07 2068 C DB 000H,004H,000H,007H
0A20 A6 2069 C DB 0A6H
0A21 60 4F 56 3A 51 60 2072 C DB 060H,04FH,056H,03AH,051H,060H
0A22 00 00 00 00 00 0C 2074 C DB 000H,000H,000H,000H,000H,00CH
0A2D 00 00 00 00 SE 2E A3 2075 C DB 000H,000H,000H,000H,00SEH,0AE3H
0A39 00 00 00 00 00 2076 C DB 000H,000H,000H,000H,000H,000H
0A3A 00 00 00 00 00 2077 C DB 000H,000H,000H,000H,000H,000H
0A40 00 00 00 00 00 2079 C DB 000H,000H,000H,000H,000H,000H
0A46 00 00 00 00 00 2081 C DB 000H,000H,000H,000H,000H,000H
0A4C 00 00 00 00 00 2083 C DB 000H,000H,000H,000H,000H,000H
0A54 00 00 00 00 00 2085 C DB 000H,000H,000H,000H,000H,000H
0A57 28 18 08 2087 C DB 40D,24D,0BD
0A5A 2000 2088 D DW 02000H
0A5C 08 GF 00 06 2090 C DB 008H,00FH,000H,006H
0A60 23 2091 C DB 023H
0A61 37 27 20 31 15 2093 C DB 037H,027H,02DH,037H,031H,015H
0A65 00 00 00 00 E1 24 2095 C DB 000H,000H,000H,000H,0E1H,024H
0A77 14 08 00 00 EF E3 2096 C DB 0C7H,014H,000H,0E0H,0F0H,0E3H
0A79 FF 2097 C DB OFFH
0A7A 00 01 02 03 04 05 2100 C DB 000H,001H,002H,003H,004H,005H
0A80 06 07 10 11 12 13 2101 C DB 006H,007H,010H,011H,012H,013H
0A86 15 16 17 01 00 2102 C DB 015H,016H,017H,001H,000H,000H
0A8E 00 00 00 00 00 2104 C DB 000H,000H,000H,000H,000H,000H
0A94 05 FF 2105 C DB 005H,0FFH,0FFH
0A97 50 18 09 2107 C DB 80D,24D,0BD
0A9A 4000 2108 D DW 04000H
0A9C 01 0F 00 06 2110 C DB 001H,00FH,000H,006H
0AA0 23 2111 C DB 023H
0AA1 70 4F 59 20 5E 05 2114 C DB 070H,04FH,059H,020H,05EH,005H
0AAD 00 00 00 00 E0 23 2116 C DB 000H,000H,000H,000H,0E0H,023H
0AB9 CF 28 00 DF EF E3 2117 C DB 0C7H,028H,000H,0DFH,0FEH,0E3H
0AB9 FF 2118 C DB OFFH
0ABA 00 01 02 03 04 05 2120 C DB 000H,001H,002H,003H,004H,005H
0AC0 06 07 10 11 12 13 2121 C DB 006H,007H,010H,011H,012H,013H
0AC6 15 16 17 01 00 2122 C DB 015H,016H,017H,001H,000H,000H
0ADC 00 00 00 00 00 2124 C DB 000H,000H,000H,000H,000H,000H
0AD4 05 FF 2126 C DB 005H,0FFH,0FFH
0AD7 50 18 0E 2127 C DB 80D,24D,14D
0ADA 8000 2129 C DB 08000H
0ADC 05 0F 00 00 2131 C DB 005H,00FH,000H,000H
0AEO A2 2133 C DB 0A2H
0AE1 60 4F 56 3A 51 60 2135 C DB 060H,04FH,056H,03AH,051H,060H
0AE7 70 1F 00 00 00 2136 C DB 070H,01FH,000H,000H,000H,000H
0AFD 00 00 00 00 SE 2E B8 2137 C DB 000H,000H,000H,000H,00SEH,0BE8H
0AF9 FF 2139 C DB OFFH
0AFA 00 08 00 08 18 2141 C DB 000H,008H,000H,008H,0018H,018H
0B00 00 00 08 00 80 2142 C DB 000H,000H,000H,008H,000H,000H
0806 03 18 00 00 0B 00 2143 C
080C 05 00 2144
080E 07 00 00 00 10 2146
0814 07 0F FF 2148
0817 50 18 OE 2149
081A 00 00 2150
081C 05 00 00 2151
0820 A7 2154
0821 58 4F 53 17 50 BA 2156
0822 6C 1F 00 00 00 00 2157
082D 00 00 00 SE 2158
0833 50 14 07 5F OA 6B 2159
0837 FF 2161
083A 00 00 00 00 00 07 2162
083E 00 00 00 00 00 01 2163
0840 00 00 00 00 00 01 2164
0844 00 00 00 00 00 01 2165
0848 00 00 00 00 00 10 2167
084C 07 FF FF 2169
= 0440
0857 50 18 OE 2175
085A 8000 2176
085C 01 0F 00 06 2177
0860 A2 2180
0861 60 4F 56 3A 50 60 2182
0862 70 1F 00 00 00 00 2183
086D 00 00 00 5E E3 2185
0873 FF 2187
087A 00 00 00 00 18 18 2188
0880 00 00 00 00 00 00 2189
0886 00 18 00 00 00 2190
088C 05 00 2191
0890 00 00 00 00 00 00 2193
0894 05 FF FF 2195
= 0497
0897 50 18 OE 2197
089A 8000 2198
089C 01 0F 00 06 2200
08A0 A7 2202
08A1 58 4F 53 37 52 00 2203
08A7 6C 1F 00 00 00 00 2205
08AD 00 00 00 5E E3 2206
08B3 FF 2207
08BA 00 00 02 03 04 05 2209
08C0 14 07 38 39 3A 3B 2211
08CC 3E 3F 01 00 2213
08CE 0F FF 2214
08D4 05 0F FF 2216
= 04C0
08D7 28 18 OE 2219
08DA 8000 2220
08DC 08 03 00 03 2222
08E0 A7 2224
08E1 20 27 28 20 28 60 2233
08E7 6C 1F 00 00 00 07 2232
08ED 00 00 00 5E E3 2234
08F3 50 14 07 5E OA A3 FF 2236
08FA 00 01 02 03 04 05 2237
08FC 01 07 38 39 3A 3B 2238
090C 3E 3F 08 00 2239
090E 00 00 00 00 00 00 2240
0914 0E 0E FF 2242
0916 0E 0E FF 2244
0918 28 18 OE 2246
091A 8000 2247
091C 08 03 00 03 2248
0920 A7 2250
0921 20 27 28 20 28 60 2253
0927 6C 1F 00 00 00 07 2252
092D 00 00 00 5E E3 2254
0933 50 14 07 5E OA A3 FF 2256
093A 00 01 02 03 04 05 2258
093E 01 07 38 39 3A 3B 2260
094C 3E 3F 08 00 2262
094E 00 00 00 00 00 16 2263
0954 0E 0E FF 2265
0956 0E 0E FF 2267
0957 50 18 OE 2268

DB 000H,018H,000H,000H,00BH,000H
DB 005H,000H
DB 000H,000H,000H,000H,000H,010H
DB 007H,00FH,0F7H
;--10--
DB 80D,24D,14D
DB 0800H
DB 005H,C0FH,000H,000H
DB 0A7H
DB 058H,04FH,053H,017H,05CH,0BAH
DB 05CH,01FH,000H,000H,000H,000H
DB 000H,000H,000H,000H,000H,000H
DB 05DH,014H,00FH,05FH,03AH,00BH
DB 0FTH
DB 000H,001H,000H,000H,000H,007H
DB 000H,000H,000H,001H,000H,000H
DB 000H,007H,000H,000H,001H,000H
DB 005H,000H
DB 000H,000H,000H,000H,000H,010H
DB 007H,00FH,0F7H
BASE_2 EQU S - VIDEO_PARMS
;------ > 16K MODE VALUES
;----- > 16K MODE VALUES
DB 80D,24D,14D
DB 0800H
DB 001H,00FH,000H,006H
DB 0A2H
DB 060H,04FH,056H,03AH,350H,060H
DB 070H,01FH,000H,000H,000H,000H
DB 000H,000H,000H,000H,000H,000H
DB 05DH,028H,00FH,05FH,06EH,0E3H
DB 0FTH
DB 000H,000H,000H,000H,018H,018H
DB 000H,000H,000H,000H,000H,000H
DB 000H,018H,000H,000H,00BH,000H
DB 005H,000H
DB 000H,000H,000H,000H,000H,000H
DB 005H,00FH,0F7H
;--10--
DB 80D,24D,14D
DB 0800H
DB 001H,00FH,000H,006H
DB 0A7H
DB 025H,04FH,053H,037H,052H,000H
DB 06CH,01FH,000H,000H,000H,000H
DB 000H,000H,000H,000H,000H,000H
DB 05DH,028H,00FH,05FH,03AH,0A3H
DB 0FTH
DB 000H,001H,002H,003H,004H,005H
DB 014H,007H,038H,039H,03AH,018H
DB 014H,007H,038H,039H,03AH,018H
DB 005H,000H
DB 000H,000H,000H,000H,000H,000H
DB 005H,00FH,0F7H
BASE_3 EQU S - VIDEO_PARMS
;------ HI RES ALTERNATE VALUES
;----- > 16K MODE VALUES
DB 40D,24D,14D
DB 0800H
DB 008H,003H,000H,003H
DB 0A7H
DB 020H,027H,028H,028H,028H,060H
DB 06CH,01FH,000H,000H,000H,000H
DB 000H,000H,000H,000H,000H,000H
DB 05DH,014H,00FH,05FH,00AH,0A3H
DB 0FTH
DB 000H,001H,002H,003H,004H,005H
DB 014H,007H,038H,039H,03AH,018H
DB 03CH,030H,038H,037H,000H,000H
DB 00FH,000H
DB 000H,000H,000H,000H,000H,010H
DB 00EH,000H,0FFH
;--1--
DB 40D,24D,14D
DB 0800H
DB 008H,003H,000H,003H
DB 0A7H
DB 020H,027H,028H,028H,028H,060H
DB 06CH,01FH,000H,000H,000H,000H
DB 000H,000H,000H,000H,000H,000H
DB 05DH,014H,00FH,05FH,028H,0A3H
DB 0FTH
DB 000H,001H,002H,003H,004H,005H
DB 014H,007H,038H,039H,03AH,018H
DB 014H,007H,038H,039H,03AH,018H
DB 005H,000H
DB 000H,000H,000H,000H,000H,000H
DB 005H,00FH,0F7H
;--2--
DB 80D,24D,14D
0C5A 1000
0C5C 01 03 00 03
0C60 A7
0C61 58 4F 53 37 51 5B
0C67 6C 1F 00 00 06 07
0C6D 00 00 00 0E 28
0C71 28 0F 5E DA A3
0C79 0F 00
0C7A 00 01 02 03 04 05
0C80 14 07 38 39 3A 3B
0C86 3C 3D 3E 3F 08 00
0C8C 00 00 00 00 10
0C8E 00 00 00 00 10
0C94 00 FF
0C97 50 18 OE
0C9A 100C
0C9C 01 03 00 03
0CA0 A7
0CA1 58 4F 53 37 51 5B
0CA7 6C 1F 00 00 06 07
0CC0 00 00 00 0E 28
0CB8 28 0F 5E DA A3
0CB9 0F 00
0CBA 00 01 02 03 04 05
0CCC 14 07 38 39 3A 3B
0CCD 3C 3D 3E 3F 08 00
0CCF 00 00 00 00 10
0CD4 00 FF
0CD7 50 18 OE
0CD9 100C
0CE0 01 03 00 03
0CEA A7
0CF1 50
0CF2 E8 C6
0CF3 32 E4
0CF6 00 00
0CF8 88 F0
0CF9 00 00 00 02 88
0CDA 72 06
0CFE 58
0D00 00 02
0D02 E9 219C R
0D05 E8 OCFE R
0D07 E8 00 FE R
0D09 2C FF AA 06EF R

COMBO_VIDEO PROC NEAR
    STI
    CLD
    PUSH DS
    PUSH ES
    PUSH DX
    PUSH BX
    PUSH SI
    PUSH DI
    PUSH AX
    MOV AL, AH
    XOR AH, AH
    MOV AX, 1
    MOV SI, AX
    MOV AX, T2L
    ADD AX, T2H
    JB NEXT
    INT 21H
    JMP V_RET
NEXT:
    ASSUME DS:ABS0
    CALL DDS
    POP AX
    RET
    WORD PTR CS:[SI + OFFSET T2]
    JMP TO AH=0 THRU AH=XX

;------ UTILITY ROUTINES

SET_DS_TO_DATA_SEGMENT
    DOS PROC NEAR
    PUSH AX
    SUB AX, AX ; SAVE REGISTER
    MOV DS, AX
    POP AX
    RET
    ENDP

WHAT_BASE PROC NEAR
    PUSH DS
    CALL DOS
    MOV DS, AX ; SAVE DATA SEGMENT
    MOV AX, 0A000H ; GET LOW MEMORY SEGMENT
    MOV DX, ADDR_6845 ; SET CRTIC ADDRESS
    AND DL, 0FH ; STRIP OFF LOW NIBBLE
    MOV AH, 0 ; SET TO STATUS REGISTER
    POP DS
    ENDP

OUT_DX PROC NEAR ; AH=INDEX, AL=DATA, DX=PORT
    XCHG AL, AH ; GET INDEX VALUE
    OUT DX, AL ; SET INDEX REG
    XCHG AL, AH ; GET DATA VALUE
    OUT DX, AL ; SET DATA REG
    DEC DX ; SET DX BACK TO INDEX
    RET
ENDP

ROUTINE TO SOUND BEEPER
    BP_1 PROC NEAR
    INT 21H
    RET
    ENDP

BEEP PROC NEAR
    PUSH DX
    MOV DX, TIMER1 ; SEL TIR 2 LSB,MSB, BINARY
    MOV AL, 1011010B ; WRITE THE "TIMER MODE" REG
    OUT BL, AL ; DIVISOR FOR 1000 Hz
    MOV AX, 533H ; WRITE TIMER 2 CNT - LSB
    CALL BP_1
    MOV AL, AH ; WRITE TIMER 2 CNT - MSB
    RET
    ENDP
0D38 EC 395 IN AL, DX ; GET SETTING OF PORT
0D39 8A E0 2196 MOV AH, AL ; SAVE THAT SETTING
0D3B 0C 31 2397 OR AL, 03 ; TURN SPEAKER ON
0D3C 88 001E R 2398 CALL BX ; SET CNT TO WAIT 500 MS
0D40 28 C9 2399 SUB CX,CX
0D42 00 2400 G7:
0D42 FE 2401 LOOP G7 ; DELAY BEFORE TURNING OFF
0D44 FE CB 2402 DEC BL ; DELAY ONE EXPRIED
0D46 FE FA 2403 JNZ G7 ; NOW TRY NEXT SPK
0D48 8A C4 2404 MOV AL,AH ; RECOVER VALUE OF PORT
0D4A E8 001E R 2405 CALL BP-1
0D4C 88 00 2406 POP DX
0D4E C3 2407 RET ; RETURN TO CALLER
0D4F BEEF 2408 ENDP

0D50 2409
0D51 2410 ;----- FIND THE PARAMETER TABLE VECTOR IN THE SAVE TABLE
0D52 2411 SET_BASE PROC NEAR
0D54 2412 ASSUME DS:ABS0
0D56 2413 CALL BX_SAVE_PTR ; GET PTR TP PTR TABLE
0D58 2414 LES BX_SAVE_PTR ES:[BX] ; GET PARAMETER PTR
0D5A 2415 RET
0D5B 2416 SET_BASE ENDP

0D5C 2417
0D5D 2418 ;----- ESTABLISH ADDRESSING TO THE CORRECT MODE TABLE ENTRY
0D5E 2419 MAKE_BASE PROC NEAR
0D60 2420 ASSUME DS:ABS0
0D62 2421 PUSH DX
0D63 2422 PUSH AX
0D64 2423 CALL BX_SAVE_PTR ; GET PARM TBL PTR
0D66 2424 MOV AH,CRT_MODE
0D68 2425 TEST INFO_060H ; TEST FOR BASE CARD
0D6A 2426 JZ B_M_1 ; MIN MEMORY
0D6B 2427
0D6C 2428 ;----- WE HAVE A MEMORY EXPANSION OPTION HERE
0D6D 2429 CMP AH,0FH
0D6F 2430 JNE B_M_2 ; ADD BX,BASE_2 - BASE_1
0D70 2431 JMP B_M_OUT
0D71 2432
0D72 2433 B_M_2:
0D73 2434 CMP AH,010H
0D75 2435 JNE B_M_3
0D76 2436 ADD BX,BASE_2 + M_TBL_LEN - BASE_1
0D78 2437 JMP B_M_OUT
0D79 2438
0D7A 2439 B_M_3:
0D7B 2440 CMP AH,03H
0D7D 2441 JA B_M_3 ; SKIP ENHANCED PORTION
0D7E 2442
0D7F 2443 ;----- CHECK THE SWITCH SETTING FOR ENHANCEMENT
0D80 2444 CMP AH,03H
0D81 2445 JE B_M_3 ; SECONDARY EMULATE SETTING
0D82 2446 CMP AH,0FH
0D84 2447 JE B_M_3 ; PRIMARY EMULATE SETTING
0D85 2448 JMP B_M_3
0D86 2449
0D87 2450 ;----- WE WILL PERFORM ENHANCEMENT
0D88 2451 BRS:
0D89 2452 ADD BX,BASE_3 - BASE_1 ; VECTOR TO ENHANCEMENT TBL
0D8A 2453 B_M_3:
0D8B 2454 MOV CL,CRT_MODE
0D8C 2455 SUB CH,CH
0D8D 2456 JCNZ B_M_4
0D8E 2457
0D8F 2458 ;----- THIS LOOP WILL MOVE THE PTR TO THE INDIVIDUAL MODE ENTRY
0D90 2459 B_M_4:
0D91 2460 ADD BX,M_TBL_LEN ; LENGTH OF ONE MODE ENTRY
0D92 2461 LOOP B_M_4
0D93 2462 B_M_4:
0D94 2463 POP DX
0D95 2464 POP CX
0D96 2465 RET
0D97 2466 MAKE_BASE ENDP
0D98 2467
0D99 2468 ;----- PROGRAM THE EGA REGISTERS FROM THE PARAMETER TABLE
0D9A 2469 SET_REGS PROC NEAR
0D9B 2470 ASSUME DS:ABS0,ES:NOTHING
0D9C 2471
0D9D 2472 ;----- PROGRAM THE SEQUENCER
0D9E 2473 CALL MAKE_BASE ; GET TABLE PTR
0D9F 2474 ADD BX,TFS_LEM ; MODE TO SEQUENCER PARMS
0DA0 2475 MOV DX,0001H ; DISABLE INTERRUPTS
0DA1 2476 CALL OUT_DX ; RESET SEQUENCER
0DA2 2477 CALL AL,ES:[BX] ; GET SEQUENCER VALUE
0DA3 2478 INC AH ; NEXT INDEX REGISTER
0DA4 2479 CALL OUT_DX ; SET IT
0DA5 2480 D1:
0DA6 2481 INC AH ; NEXT INDEX REGISTER
0DA7 2482 MOV BX,0100H ; NEXT TABLE ENTRY
0DA8 2483 CALL OUT_DX ; START SEQUENCER
0DA9 2484 CALL AL,ES:[BX] ; ENABLE INTERRUPTS
0DAA 2485 INC BX ; CRTC INDEX REGISTER
0DAB 2486 MOV DX,ADDR_6845 ; CRTC INDEX REGISTER
0DAC 2487 SUB AH,AH ; COUNTER
0DAD 2488 X1:
0DAE 2489 MOV AL,ES:[BX] ; GET VALUE FROM TABLE
0DAF 2490 CALL OUT_DX ; SELECT REGISTER
0DB0 2491 INC BX ; NEXT TABLE ENTRY
0DB1 2492 INC AH ; NEXT INDEX VALUE
0DB2 2493 CMP AH,Nh ; TEST REGIST COUNT
8DFH   F2 F2
8DF6   0B 4F F1
8DFA   E6 E0
8DFC   A3 0460 R
80F9
80F7   B3 59
80F6   11 0E
80F5   0B C0
80F4   2A E4
80F3   2D 03 D3: 
80F2   26 8A 07
80F1   EE E0
80F0   EE E0
80EF   EE E0
80E9   8E 60
80E8   EE E0
80E7   EE E0
80E6   43
80E5   FE CF
80E4   72 EF
80E3   8E 00
80E2   EE
80E1   EE
80E0   EE
80E9   PUSH DS
80E8   PUSH DS
80E7   LES DI, SAVE_PTR
80E6   LES DI, DWORD PTR ES:[DI][4]
80E5   MOV AX, ES
80E4   OR AX, DI
80E3   JZ SAVE_OUT
80E2   ;---- STORE AWAY THE PALETTE VALUES IN RAM SAVE AREA
80E1   PUSH DS
80E0   PUSH DS
80E9   POP DS
80E8   POP DS
80E7   MOV CX, 16D
80E6   MOV [SI], SAVREGS
80E5   MOV [SI+1], OVERSCAN
80E4   POP DS
80E3   ;---- PROGRAM THE GRAPHICS CHIPS
80E2   MOV DL, GRAPH_1_PGS
80E1   OUT DX, AL
80E0   MOV DL, GRAPH_2_PGS
80E9   OUT DX, AL
80E8   MOV DL, GRAPH_ADDR
80E7   OUT DX, AL
80E6   SUB AH, AH
80E5   ;---- MODE SET REGEN CLEAR ROUTINE
80E4   BLANK PROC NEAR
80E3   ASSUME DS:ABS0, ES:NOTHING
80E2   TEST AL, 080H
80E1   JNZ CLEAR
80E0   MOV DX, 0B800H
80E9   MOV AL, CRT_MODE
80E8   CMP AL, 6
80E7   JE SET_6
80E6   MOV DX, 0B000H
80E5   MOV AL, CRT_MODE
80E4   CMP AL, 0
80E3   JE SET_0
80E2   MOV DX, 0A000H
80E1   JE SET_0
80E0   MOV DX, 0A000H
80E9   ; FILL REGEN WITH BLANKS
80E8   ; SEE IF BLANK IS TO OCCUR
80E7   ; MODE SET HIGH BIT
80E6   ; SET BLANK FOR REGEN
80E5   ; COLOR MODE SET ADDRESS
80E4   ; CURRENT MODE SET
80E3   ; 0-6 ARE COLOR MODES
80E2   ; MONOCHROME REGEN ADDRESS
80E1   ; MONOCHROME MODE
80E0   ; REMAINING MODES
80E9   ; ALPHA BLANK VALUE
80E8   ; ALPHA MODES 0-3
80E7   ; ALPHA MODE
80E6   ; GRAPHICS BLANK VALUE
80E5   ; SET THE REGEN SEGMENT
80E4   SRLOADED
80E3   MOV ES, DX
80E2   MOV CX, CRT_LEN
80E1   JCZ OUT_1
80E0   MOV BX, 0A000H
80E9   CMP DX, 0A000H
80E8   JE N_BA
80E7   MOV CR_OUOH
80E6   ; BLANK VALUE
80E5   ; CLEAR POINTER
80E4   ; CLEAR THE PAGE
80E3   ; RETURN TO CALLER
80E2   ;---- SEE IF WE ARE TO SUPPORT 640 X 350 ON A 640 X 200 MODE
80E1   BRST_DET PROC NEAR
80E0   ASSUME DS:ABS0
80E9   PUSH DS
80E8   CALL INFO_3
80E7   AND AL, 0F0H
80E6   CMP AL, 03H
80E5   ; EMULATE MODE
80E4   ; EMULATE MODE
OEB3  FA
OEB4 C7 06 010C R 0000 E
OEB5 BC 0E 010E R....
OEB7 80 26 0487 R F3
OEC5 00 C5 0485 R 00
OEGF 0E 0E 0487 R 02 ....
OEC5 0E 0F 0100 R ...
OEGC 71 71 0101 R...
ODEB 30 D0 40
DEBD 74 4B
OEC9  CD 98 2194E R
OEF3 CD 42
OEF8 AI 8100 R ...
ODED 24 3D
OEGF 7A 40
OFC7 C5 06 048B R 18
OFO5 C7 06 0485 R 0000 ...
OFC5 5B
OFE9 83 62 CD 42 F ...
OFOF C7 06 0460 R B0C
OFT5 80 04 OE 0487 R 08 ...
OF7A E9 19 EF9
OFE1 FA 58
OFEF E3 89 0170 9E 0124
OFE4 A2 0A
OFHC BF 8E '6 ... 3A
OFE3 EB 9C 90
OFE1 A0 58 ...
OFE4 EA 80
OFE3 06 23 0494 R
OFBC 48 BD 09 ...
OFE7 0A 89 04 OE 06 ...
OFE3 04 89 04 OE 04 ...
OFE7 58 16 16 04 ...
OF5C C9 0F6 048R 07 ...
OF45 C6 06 048R 0 ...
OF91 7B 16 0 ...
OF7A 16 58 ...
OFE4 0C ...
OF58 73 A7 24 ...
OFE7... 8A 89 47 01
OFE8 ... 7A 01 48 ...
OF68 26 ... 47 43 ... 00 04 AR ...
OF85 2B 48 ...
OF8C 5B 4A 83 48 83
OFE8 3A 03 9E 8A ...
OF7F 2A 08 ...
OFB5... 26 48 ...
OF88 C7 2A 16 01 B5 ...
OFA3 0D 4A 49 05 ...
OFA0 26 4B 77 3A ...
OF2A 80 FC 03    2773   CMP AH,03H
OF53 77 35       2774   JA ENTRY_1
OF54 E8 0E9A R   2775   CALL BRST_DET
OF5A 72 02       2776   JC ENTRY_2
0FAC 80 02       2777
0FAE 80 02       2778
0FAE E8 1EA E     2779   ENTRY_2:
0FB1 80 02       2780   MOV AL,2 ; COLOR ALPHA CHAR GEN
0FB4 80 02       2781   CALL CH_GEN
0FB7 80 02       2782   CALL DOS
0FB4 80 02       2783   MOV AH,CRT_MODE
0FB8 80 02       2784   CMP AH,7 ; IS IT MONOCHROME
0FBB 80 02       2785   JL CRT_MODE
0FBD EB 1D 90    2786   JMP ENTRY_1
0FC0 80 02       2787   FDC_IT;
0FC0 BD 0000 E   2788   BP_OFFSET COMM_FDG
0FC3 BB 0E00     2789   MOV BX,OEOOH ; TABLE POINTER
0FC6 80 02       2790   MOV BX,OEOOH ; 14 BYTES PER CHAR
0FC9 80 02       2791   PUSH CS
0FCB 80 02       2792   POP ES
0FCC 80 02       2793   MOV DX,ES:[BP]
0FCE 74 0C       2794   OR DX,DX
0FD0 80 0001     2795   MOV DX,ENTRY_1
0FD3 80 02       2796   INC BX
0FD6 80 02       2797   CALL DB_MAPPZ
0FD9 80 02       2798   ADD BP,010D
0FDC 80 02       2799   JMP FDC
0FDE 80 02       2800   ENTRY_1:
0FDF 80 02       2801   CALL SET_REGS
0FE0 80 02       2802   CALL BLANK
0FE3 80 02       2803   CALL PILS
0FE6 80 02       2804   CALL PILS
0FE9 80 02       2805   ASSUME DS:ABSO
0FEB 80 02       2806   CALL DDS
0FED 80 02       2807   CMP CRT_MODE,OFH
0FF0 80 02       2808   JB MS_1
0FF3 C7 06 010C R 0000 E 2810   MOV WORD PTR GRX_SET , OFFSET COMM
0FF5 80 02       2811   MS_1:
0FF7 80 02       2812   CMP CRT_MODE,7
0FF9 77 09       2813   JA SAVE_GRPH
0FFB 80 02       2814   CMP CRT_MODE,3
0FFD 80 02       2815   JBE SAVE_ALPH
1000 80 02       2816   SAVE_GRPH:
1002 C1 1E 04A8 R 2818   LES BX,SAVE_PTR
1005 80 02       2819   ADD BX,00H
1007 80 02       2820   LES BX,DWORD PTR ES:[BX]
100A 80 02       2821   MOV AX,ES
100C 80 02       2822   OR AX,BX
100E 74 32       2823   JZ J4J ; JMP AHO_DONE
1010 BE 0007     2824   MOV SI,07H
1013 80 02       2825   SG_1:
1015 80 02       2826   CMP AL,[ES:[BX][SI]]
1017 80 02       2827   JE AHO_DONE
1019 80 02       2828   CMP AL,CRT_MODE
101B 80 02       2829   JE AHO_DONE
101D 80 02       2830   INC SI
101F 80 02       2831   INC SI
1021 80 02       2832   JMP SG_1
1023 80 02       2833   SG_2:
1025 80 02       2834   CLI
1027 80 02       2835   MOV AL,BYTE PTR ES:[BX]
1029 80 02       2836   DEC AL
102B 80 02       2837   MOV AX,WORD PTR ES:[BX][1]
102D 80 02       2838   MOV AX,WORD PTR ES:[BX][3]
102F 80 02       2839   MOV AX,WORD PTR ES:[BX][5]
1031 80 02       2840   MOV WORD PTR GRX_SET,AX
1033 80 02       2841   MOV AX,WORD PTR ES:[BX][7]
1035 80 02       2842   MOV WORD PTR GRX_SET,AX
1037 80 02       2843   STI
1039 80 02       2844   J4J: SHORT AHO_DONE
103B 80 02       2845   SAVE_ALPH:
103D 80 02       2846   LES BX,SAVE_PTR
103F 80 02       2847   ADD BX,0BH
1041 80 02       2848   LES BX,DWORD PTR ES:[BX]
1043 80 02       2849   MOV AX,BX
1045 80 02       2850   OR AX,BX
1047 80 02       2851   AHO_DONE:
1049 80 02       2852   MOV SI,DBH
104B 80 02       2853   SA_1:
104D 26 8A CO     2855   MOV AL,ES:[BX][SI]
1051 80 02       2856   CMP AL,0FH
1053 77 09       2857   JA AHO_DONE
1055 80 02       2858   CMP AL,CRT_MODE
1057 74 03       2859   JE SA_2
1059 80 02       2860   INC SI
105B 80 02       2861   JMP SA_1
105D 26 8A 27     2863   SA_2:
1061 80 02       2864   MOV AH,ES:[BX]
1063 80 02       2865   MOV AL,ES:[BX][1]
1065 80 02       2866   MOV DX,ES:[BX][2]
1067 80 02       2867   MOV BP,ES:[BX][6]
1069 80 02       2868   MOV ES:[BX][8],BP
106B 80 02       2869   PUSH BX
106D 80 02       2870   MOV AX,110H
106F 80 02       2871   INT 10H
1071 80 02       2872   MOV AL,ES:[BX][0AH]
1073 80 02       2873   CMP AL,0FH
1075 80 02       2874   JE AHO_DONE
1077 80 02       2875   DEC AL
1079 80 02       2876   MOV WORD AL,DS:[AL]
107B 80 02       2877   MOV DS:[AL],AL
107D 80 02       2878   MOV DS:[AL],AL
107F 80 02       2879   MOV DS:[AL],AL
1081 80 02       2880   MOV DS:[AL],AL
1083 80 02       2881   MOV DS:[AL],AL
1085 80 02       2882   MOV DS:[AL],AL
1087 80 02       2883   AHO_DONE:
1089 80 02       2884   CALL DDS
108B 80 02       2885   CMP CRT_MODE,7
108D 77 1E       2886   JA DNDCS
108F 80 02       2887   MOV BP_OFFSET,COMPAT_MODE
1091 80 02       2888   MOV AL,CRT_MODE
1093 80 02       2889   SUB AL,0FH
1095 80 02       2890   ADD BX,AX
1097 80 02       2891   MOV AL,CS:[BX]
1099 80 02       2892   MOV AL,030H
109B 80 02       2893   CMP CRT_MODE,6
109D 80 02       2894   JE DO_PAL
109F 80 02       2895   MOV AL,03FH
10A1 80 02       2896   MOV CRT_PALETTE,AL
10A3 80 02       2897   DO_PAL:
10A6 A2 0466 R   2898   MOV CRT_PALETTE,AL

;------ SET THE LOW RAM VALUES FOR COMPATIBILITY (3DB AND 3D9 SAVE BYTES)
10C1 28 0E 0460 R 2899 DDNDCS: MOV CX,CURSOR_MODE
10C1 28 0E 0460 R 2900 JMP AH1
10C5 EB 28 90 2901
10C8 20 28 20 29 2A 2E 2902 COMPAT_MODE LABEL BYTE
10CE 1E 29 2903 DB 02BH,028H,02DH,029H,02AH,02EH
10D0 80 FD 00 2904
10D3 75 01 2905 DB 01EH,029H
10D6 EB C1 2906 INC V1+5,INC
10D7 EB 0A 2907 SUBTL V1-5,INC
10D9 20 09 2908 PAGE
10DA 20 09 2909
10DD 20 09 2910 CALC_CURSOR PROC NEAR
10DE 8B 06 DB,AB80 2911 MOV CH,DB
10E0 8B 07 DB,AB81 2912 CMP CL,CH
10E2 74 F1 2913 JNC CC_1 ;CHECK FOR FULL HEIGHT
10E4 8B 06 DB,AB80 2914 MOV NO,CL ;NORMAL CHECK
10E7 8B 07 DB,AB81 2915 INC NO ;ADJUST END VALUE
10EA 8B 06 DB,AB80 2916 SUB CL,CL
10EB 8B 07 DB,AB81 2917 INC CC_1 ;ADJUST FOR EGA REGISTERS
10ED 8B 06 DB,AB80 2918 CMP CL,BYTE PTR POINTS ;WILL NOT MAP
10EE 8B 07 DB,AB81 2919 JB AH1 ;NO BITS ON
10F0 8B 06 DB,AB80 2920 SUB CL,CL ;EGA METHOD FOR CURSOR END
10F2 8B 07 DB,AB81 2921 PUSH CX ;SAVE CURSOR TYPE VALUE
10F4 8B 06 DB,AB80 2922 SUB CL,CH ;END - START
10F6 8B 07 DB,AB81 2923 CMP CL,010H ;LONG DOUBLE EQUAL
10F8 8B 06 DB,AB80 2924 POP CX ;RESTORE
10FA 8B 07 DB,AB81 2925 JNE COMP_N ;ADD 1 FOR CORRECT CURSOR
10FB FE C1 2926 INC CL ;BACK TO CALLER
10FC 33 C3 2927 COMP_N:
10FE 8B 06 DB,AB80 2928 CALC_CURSOR ENDP
10FF 8B 07 DB,AB81 2929
10004 2930
1000F 2931 SET_CTYPE SET CURSOR TYPE
10010 2932 THIS ROUTINE SETS THE CURSOR VALUE
10013 2933 INPUT (CX) HAS CURSOR VALUE CH-START LINE, CL-STOP LINE
10016 2934 OUTPUT NONE
10019 2935 CUT_OFF EQU 4
1001C 2936 AH1: ASSUME DS:ABS()
1001E 2937 MOV AH,CURSR_START ;CRTC REG FOR CURSOR SET
10020 2938 MOV CURSOR_MODE,CX ;SAVE IN DATA AREA
10022 2939 TEST SYMBL,010H ;EGA ACTUAL BIT
10024 2940 JNZ DO_SET ;0=EGA, 1=OLD CARDS
10026 2941
10027 2942 ---- THIS SECTION WILL EMULATE CURSOR OFF ON THE EGA
10029 2943 MOV AL,CN ;GET START VALUE
1002B 2944 AND AL,060H ;TURN OFF CURSOR ?
1002D 2945 CMP AL,020H ;TEST THE BITS
1002F 2946 JNE SKIP_CURSOR_OFF ;SKIP CURSOR OFF
10031 2947 MOV CX,0100H ;EMULATE CURSOR OFF
10033 2948 SHORT DO_SET
10035 2949
10036 2950 ---- THIS SECTION : ADJUST THE CURSOR And TEST FOR ENHANCED OPERATION
10038 2951 AH1_A: TEST INFO_1 ;CURSOR EMULATE BIT
1003A 2952 JNZ DO_SET ;0=EMULATE, 1=VALUE AS-IS
1003C 2953 CMP CRT_MODE,3 ;POSSIBLE EMULATION
1003E 2954 JAE AH1_B ;NO SET IF CURSOR TYPE
10040 2955 CALL BRST_DET ;SEE IF EMULATE MODE
10042 2956 JNC AH1_S ;NOT EMULATING
10044 2957 CMP AH,CUT_OFF ;TEST
10046 2958 JBE AH1_B ;SKIP ADJUST
10048 2959 ADD CH,5 ;ADJUST
1004A 2960
1004B 2961 AH1_B: CMP CL,CUT_OFF ;TEST END
1004D 2962 JBE SKIP_ADJUST ;SKIP ADJUST
1004F 2963 ADD CL,5 ;ADJUST
10051 2964
10052 2965 AH1_S: CALL CALC_CURSOR ;ADJUST END REGISTER
10054 2966 DO_SET: CALL M16 ;OUTPUT CX REG
10056 2967 JMP _RET ;RETURN TO CALLER
10058 2968
10059 2969 ---- THIS ROUTINE OUTPUTS THE CX REGISTER TO THE CRTC REGS NAMED IN AH
1005B 2970 M16: MOV DX,ADDR 6845 ;ADDRESS REGISTER
1005D 2971 MOV AL,CH ;DATA
1005F 2972 CALL OUT_DX ;OUTPUT THE VALUE
10061 2973 INC AH ;NEXT REGISTER
10063 2974 MOV AL,CL ;SECOND DATA VALUE
10065 2975 CALL OUT_DX ;OUTPUT THE VALUE
10067 2976 PET ;ALL DONE
10069 2977
1006A 2978 POSITION THIS SERVICE ROUTINE CALCULATES THE REGEN BUFFER
1006C 2979 ADDRESS OF A CHARACTER IN THE ALPHA MODE
1006E 2980 INPUT AX = ROW, COLUMN POSITION
10070 2981 OUTPUT AX = OFFSET OF CHAR POSITION IN REGEN BUFFER
10072 2982 POSITION PROC NEAR
10074 2983 PUSH BX ;SAVE REGISTER
10075 2984 MOV BX,AX ;ROWS TO ALL
10077 2985 MOV AH,BH ;Determine bytes to row
10079 2986 MUL BYT PTR CRT_COLS ;Determine bytes to row
1007B 2987 XOR SH, BH ;Zero out
1007D 2988 ADD AH,DX ;Add column value
1007F 2989 SAL AX,2 ;* 2 for attribute bytes
10081 2990 POP BX ;RESTORE REGISTER
10082 2991 POSITION ENDP
10084 2992
10086 2993 SET_CPOS SET CURSOR POSITION
10088 2994 THIS ROUTINE SETS THE CURRENT CURSOR POSITION TO THE
1008A 2995 NEW X-Y VALUES PASSED
1008C 2996 INPUT DX = ROW,COLUMN OF NEW CURSOR
1008E 2997 BH = DISPLAY PAGE OF CURSOR
10090 2998 OUTPUT CURSOR IS SET AT CRTIC IF DISPLAY PAGE IS CURRENT
10092 2999 DISPLAY
10094 3000 AH2: CALL SET_CPOS
115A E9 219E R
115D 8A CF
115F 8A ED
1161 D1 E1
1163 8A 11
1165 89 94 0450 R
1169 3E 0462 R
116B 8A 05
116F 8A C2
1171 E8 1175 R
1174 C3

JMP V_RET

SET_CPOS:
MOV CL,BH
MOV CH,0H ; ESTABLISH LOOP COUNT
SAL CX,1 ; WORD OFFSET
MOV SI,CX ; USE INDEX REGISTER
ADD [SI,OFFSET CURSOR_POSN],DX ; USE THE POSN
CMP ACTIVE_PAGE,BH ; SET_CURSOR_RETURN
JNZ M17
MOV AX,0H ; GET ROW/COLUMN TO AX
CALL M18 ; CURSOR_SET
SET_CRSR_RETURN
RET

;----- SET CURSOR POSITION, AX HAS ROW/COLUMN FOR CURSOR
M18 PROC NEAR
MOV CX,AH ; DETERMINE LOC IN REGEN
ADD CX,CRT_START ; ADD IN THE START ADDR
MOV CX,1 ; FOR THIS PAGE
ADD CX,AH ; /2 FOR CHAR/PIXEL COUNT
CALL M16 ; SET_CURSOR_LOC_HIGH ; SETTER NUMBER FOR CURSOR
RET
M18 ENDP

READ_CURSOR:
; ROUTINE READS THE CURRENT CURSOR VALUE FROM MEMORY AND SENDS IT BACK TO THE CALLER
INPUT BH = PAGE OF CURSOR
OUTPUT BX = ROW, COLUMN OF THE CURRENT CURSOR POSITION
CX = CURRENT CURSOR MODE

AHh:
MOV BL,BH ; PAGE VALUE
XOR BH,BH ; ZERO UPPER BYTE
MOV DX,[BX+OFFSET CURSOR_POSN] ; GET CURSOR FOR THIS PAGE
MOV CX,CURSOR_MODE ; GET THE CURSOR MODE
POP DI
POP SI
POP BX
POP AX
POP DS
POP ES
POP BP
IRET

;----- READ LIGHT PEN POSITION
AHh:
MOV AL,CRT_MODE
CMP AL,07H ; READ_LPEN
JA READ_LPEN
TEST INFO,01H ; EGA_IS_COLOR
JZ MONOCHROME_HERE (MONOC BIT 1)
CMP AL,07H ; READ_LPEN
JMP OLD_LP
;----- MONOCHROME HERE (MONOC BIT 1)
CMP AL,07H ; READ_LPEN
JMP OLD_LP
;----- EGA IS COLOR HERE (MONOC BIT 0)
EGA_IS_COLOR:
CMP AL,06H ; READ_LPEN
JBE READ_LPEN
OLD_LP:
INT 42H ; CALL EXISTING CODE
POP SI
ADD SP,6 ; DISCARD SAVED BX,CX,DX
POP ES
POP BP
IRET

LIGHT PEN
; ROUTINE TESTS THE LIGHT PEN SWITCH AND THE LIGHT PEN TRIGGER. IF BOTH ARE SET, THE LOCATION OF THE LIGHT PEN IS DETERMINED. OTHERWISE, A RETURN WITH NO INFORMATION IS MADE.
OR EXIT:
(AH) = 0 IF NO LIGHT PEN INFORMATION IS AVAILABLE
(AH) = 1 IF LIGHT PEN IS AVAILABLE
(DH,DL) = POSITION OF CURRENT LIGHT PEN
(CH) = RASTER POSITION (OLD MODES)
(CX) = BEST GUESS AT PIXEL HORIZONTAL POSITION
(BX) = BEST GUESS AT PIXEL VERTICAL POSITION
ASSUME CS:CODE,DS:ABS0
V1:
LABEL
DB 00H,00H,00H,00H,00H,00H,00H,00H ; 0-5
DB 00H,00H,00H,00H,00H,00H,00H,00H ; 6-11
DB 00H,00H,00H,00H,00H,00H,00H,00H ; 12-17
DB 00H,00H,00H,00H,00H,00H,00H,00H ; 18-19
READ_LPEN PROC NEAR
;----- WAIT FOR LIGHT PEN TO BE DEPRESSED
MOV DX,ADDR_5845 ; GET BASE ADDRESS OF 6845
ADD DX,6 ; POINT TO STATUS REGISTER
IN AL,DX ; GET STATUS REGISTER
TEST AL,4 ; TEST LIGHT PEN SWITCH
SET NO LIGHT PEN RETURN
NOT SET, RETURN
;----- NOW TEST FOR LIGHT PEN TRIGGER
V9:
TEST AL,2 ; TEST LIGHT PEN TRIGGER
11EE 75 03           3151   C    JNZ V7A ; RETURN WITHOUT RESETTING TRIGGER
11F0 E9 129B R        3152   C    JMP V7 ; EXIT LIGHT PEN ROUTINE
11F3 84 16            3153   C    MOV AH,16 ; LIGHT PEN REGISTERS
11F3 B4 10            3154   C    VTA: ; ---- INPUT REGS POINTED TO BY AH, AND CONVERT TO ROW COLUMN IN DX
11F5 BB 16 0463 R     3155   C    MOV DX,ADDR_6845 ; ADDRESS REGISTER
11F9 8A C9            3156   C    MOV AL,AH ; REGISTER TO READ
11FC EE              3157   C    OUT DX,AL ; SET DATA REGISTER
11FD 50              3158   C    INC DX ; DATA REGISTER
11FF 8C 0E            3159   C    PUSH AX ; GET THE VALUE
11FF 8A EB            3160   C    MOV CH,AL ; SAVE IN CX
1200 58              3161   C    POP AX ; ADDRESS REGISTER
1202 8A 0A            3162   C    MOV AL,AH ; ADDRESS REGISTER
1203 FE C9            3163   C    INC AH ; SECOND DATA REGISTER
1207 8A 0E            3164   C    MOV AL,AH ; SECOND DATA REGISTER
1208 42              3165   C    INC DX ; POINT TO DATA REGISTER
120A 8A E5            3166   C    MOV AH,CH ; SET THE 2ND DATA VALUE
120A 8A E5            3167   C    MOV AH,CH ; AX HAS INPUT VALUE
120C 8A 1E 0449 R     3168   C    ; ---- AX HAS THE VALUE READ IN FROM THE 6845
1210 2A FF            3169   C    SUB BH,BH ; MODE VALUE TO BX
1212 8A 9F 11C1 R     3170   C    MOV BL,C5:V1(BX) ; AMOUNT TO SUBTRACT
1215 8A 0E            3171   C    SUB BX,BX ; FROM IT EVERY
1217 8A 0E            3172   C    SUB BX,BX ; SCREEN ADDRESS
1219 8A 0E            3173   C    SUB BX,BX ; DIVIDE BY 2
121B 8A 03            3174   C    SUB AX,AX ; ADJUST TO ZERO START
121D 79 02            3175   C    JNS V2 ; IF POSITIVE, GET MODE
121F 8A 03            3176   C    SUB AX,AX ; -0 PLAYS AS 0
1221 8A 03            3177   C    SUB AX,AX ; ---- DETERMINE MODE OF OPERATION
1223 8A 01            3178   C    V2: ; DETERMINE MODE
1225 8A 01            3179   C    MOV CL,3 ; SET MS SHIFT COUNT
1227 80 3E 0449 R O4   3180   C    CMP CRT_MODE,4 ; GRAPHICS OR ALPHA
122C 74 46            3181   C    JB V4 ; ALPHA_PEN
122E 80 3E 0449 R O7   3182   C    CMP CRT_MODE,7 ; ALPHA_PEN
1230 74 46            3183   C    JE V4 ; ALPHA_PEN
1232 80 3E 0449 R O6   3184   C    CMP CRT_MODE,6H ; OLD GRAPHICS MODES
1236 74 28            3185   C    JNE V8 ; NOT HIGH_RES
1238 80 3E 0449 R O2   3186   C    CMP CRT_MODE,2 ; NOT HIGH_RES
123C 74 02            3187   C    JNE V8 ; NOT HIGH_RES
123E 80 3E 0449 R O1   3188   C    CMP CRT_MODE,1 ; NOT HIGH_RES
1242 74 01            3189   C    JNE V8 ; NOT HIGH_RES
1244 8A EB            3190   C    ; ---- DETERMINE GRAPHIC ROW POSITION
1246 8A DC            3191   C    ADD CH,CH ; SAVE ROW VALUE IN CH
1248 8A 0E            3192   C    SUB BH,BH ; #2 FOR EVEN/ODD FIELD
124A 8A 0E            3193   C    SUB BX,BX ; COLUMN VALUE TO BX
124C 8A 0E            3194   C    SUB BX,BX ; #4 FOR MEDIUM RES
124E 8A 0E            3195   C    SUB BX,BX ; NOT HIGH_RES
1250 8A 0E            3196   C    SUB BX,BX ; SHIFTS VALUE FOR HIGH RES
1252 8A 0E            3197   C    SUB BX,BX ; COLUMN VALUE FOR HIGH RES
1254 8A 0E            3198   C    SUB BX,BX ; NOT HIGH_RES
1256 8A 0E            3199   C    SUB BX,BX ; #16 FOR HIGH RES
1258 8A 0E            319A   C    SUB BX,BX ; ---- DETERMINE ALPHA CHAR POSITION
125A 8A DH            319B   C    MOV DL,AH ; COLUMN VALUE FOR RETURN
125C 8A F0            319C   C    MOV DH,AL ; ROW, COL FOR RETURN
125E 8A 0E            319D   C    SUB BH,BH ; DIVIDE BY 4
1260 8A 0E            319E   C    SUB BX,BX ; FOR VALUE IN 0-24 RANGE
1262 8A 0E            319F   C    SUB BX,BX ; LIGHT_PEN_RETURN_SET
1264 8A 0E            3200   C    SUB BX,BX ; ---- NEW GRAPHICS MODES
1266 8A 0E            3201   C    SUB BX,BX ; PREPARE TO DIVIDE
1268 8A 0E            3202   C    SUB BX,BX ; AX = ROW, DX = COLUMN
126A 8A 0E            3203   C    SUB BX,BX ; SAVE REMAINDER
126C 8A 0E            3204   C    SUB BX,BX ; PEL ROW
126E 8A 0E            3205   C    SUB BX,BX ; SAVE REMAIN DIVIDE
1270 8A 0E            3206   C    SUB BX,BX ; PREPARE TO DIVIDE
1272 8A 0E            3207   C    SUB BX,BX ; DIVIDE BY BYTES/CHAR
1274 8A 0E            3208   C    SUB BX,BX ; RECEIVE
1276 8A 0E            3209   C    SUB BX,BX ; CHARACTER ROW
1278 8A 0E            320A   C    SUB BX,BX ; ---- ALPHA MODE ON LIGHT PEN
127A 8A 0E            320B   C    SUB BX,BX ; ALPHA_PEN
127C 8A 0E            320C   C    SUB BX,BX ; ROW, COL FOR VALUE
127E 8A 0E            320D   C    SUB BX,BX ; ROW, COL TO DH
1280 8A 0E            320E   C    SUB BX,BX ; COLS TO DL
1282 8A 0E            320F   C    SUB BX,BX ; COLUMN VALUE
1284 8A 0E            3210   C    SUB BX,BX ; TO BX
1286 8A 0E            3211   C    SUB BX,BX ; ---- V5:
1288 8A 0E            3212   C    SUB BX,BX ; LIGHT_PEN_RETURN_SET
128A 8A 0E            3213   C    SUB BX,BX ; INDICATE EVERYTHING SET
128C 8A 0E            3214   C    SUB BX,BX ; LIGHT_PEN_RETURN
128E 8A 0E            3215   C    SUB BX,BX ; SAVE TRUE VALUE
1290 8A 0E            3216   C    SUB BX,BX ; (IN CASE)
1292 8A 0E            3217   C    SUB BX,BX ; GET BACK ADDRESS
1294 8A 0E            3218   C    SUB BX,BX ; SAVE TO THESE PARM
1296 8A 0E            3219   C    SUB BX,BX ; ADDRESS, NOT DATA,
1298 8A 0E            321A   C    SUB BX,BX ; IF NOT WANT
129A 8A 0E            321B   C    SUB BX,BX ; RECOVER VALUE
129C 8A 0E            321C   C    SUB BX,BX ; RETURN_NO_RESET
129E 8A 0E            321D   C    SUB BX,BX ; DISCARD SAVED BX,CX,DX
12A0 8A 0E            321E   C    SUB BX,BX ; ---- V6:
12A2 8A 0E            321F   C    SUB BX,BX ; PUSH DX
12A4 8A 0E            3220   C    SUB BX,BX ; DX,ADDR_6845
12A6 8A 0E            3221   C    SUB BX,BX ; OUT DX,AL
12A8 8A 0E            3222   C    SUB BX,BX ; POP DX
12AA 8A 0E            3223   C    SUB BX,BX ; POP SI
12AC 8A 0E            3224   C    SUB BX,BX ; ADD SP,,6
12AE 8A 0E            3225   C    SUB BX,BX ; POP DS
12B0 8A 0E            3226   C    SUB BX,BX ; POP ES
12B2 8A 0E            3227   C    SUB BX,BX ; POP BP
12B4 8A 0E            3228   C    SUB BX,BX ; IRET
12B6 8A 0E            3229   C    SUB BX,BX ; ---- V7:
12B8 8A 0E            322A   C    SUB BX,BX ; READ_LPEN
12BA 8A 0E            322B   C    SUB BX,BX ; ENDP
ACT_DISP_PAGE  SELECT ACTIVE DISPLAY PAGE
THIS ROUTINE SETS THE ACTIVE DISPLAY PAGE, ALLOWING
FOR MULTIPLE PAGES OF DISPLAYED VIDEO.
INPUT
AL HAS THE NEW ACTIVE DISPLAY PAGE
OUTPUT
THE CRTC IS RESET TO DISPLAY THAT PAGE

AH5:
2877
2878
2879
2880
2881
2882
2883
2884
2885
2886
2887
2888
2889
2890
2891
2892
2893
2894
2895
2896
2897
2898
2899
2900
2901
2902
2903
2904
2905
2906
2907
2908
2909
2910
2911
2912
2913
2914
2915
2916
2917
2918
2919
2920
2921
2922
2923
2924
2925
2926
2927
2928
2929
2930
2931
2932
2933
2934
2935
2936
2937
2938
2939
2940
2941
2942
2943
2944
2945
2946
2947
2948
2949
2950
2951
2952
2953
2954
2955
2956
2957
2958
2959
2960
2961
2962
2963
2964
2965
2966
2967
2968
2969
2970
2971
2972
2973
2974
2975
2976
2977
2978
2979
2980
2981
2982
2983
2984
2985
2986
2987
2988
2989
2990
2991
2992
2993
2994
2995
2996
2997
2998
2999
3000
3001
3002
3003
3004
3005
3006
3007
3008
3009
3010
3011
3012
3013
3014
3015
3016
3017
3018
3019
3020
3021
3022
3023
3024
3025
3026
3027
3028
3029
3030
3031
3032
3033
3034
3035
3036
3037
3038
3039
3040
3041
3042
3043
3044
3045
3046
3047
3048
3049
3050
3051
3052
3053
3054
3055
3056
3057
3058
3059
3060
3061
3062
3063
3064
3065
3066
3067
3068
3069
3070
3071
3072
3073
3074
3075
3076
3077
3078
3079
3080
3081
3082
3083
3084
3085
3086
3087
3088
3089
3090
3091
3092
3093
3094
3095
3096
3097
3098
3099
3100
3101
3102
3103
3104
3105
3106
3107
3108
3109
3110
3111
3112
3113
3114
3115
3116
3117
3118
3119
3120
3121
3122
3123
3124
3125
3126
3127
3128
3129
3130
3131
3132
3133
3134
3135
3136
3137
3138
3139
3140
3141
3142
3143
3144
3145
3146
3147
3148
3149
3150
3151
3152
3153
3154
3155
3156
3157
3158
3159
3160
3161
3162
3163
3164
3165
3166
3167
3168
3169
3170
3171
3172
3173
3174
3175
3176
3177
3178
3179
3180
3181
3182
3183
3184
3185
3186
3187
3188
3189
3190
3191
3192
3193
3194
3195
3196
3197
3198
3199
3200
3201
3202
3203
3204
3205
3206
3207
3208
3209
3210
3211
3212
3213
3214
3215
3216
3217
3218
3219
3220
3221
3222
3223
3224
3225
3226
3227
3228
3229
3230
3231
3232
3233
3234
3235
3236
3237
3238
3239
3240
3241
3242
3243
3244
3245
3246
3247
3248
3249
3250
3251
3252
3253
3254
3255
3256
3257
3258
3259
3260
3261
3262
3263
3264
3265
3266
3267
3268
3269
3270
3271
3272
3273
3274
3275
3276
3277
3278
3279
3280
3281
3282
3283
3284
3285
3286
3287
3288
3289
3290
3291
3292
3293
3294
3295
3296
3297
3298
3299
3300
3301
3302
3303
3304
3305
3306
3307
3308
3309
3310
3311
3312
3313
3314
3315
3316
3317
3318
3319
3320
3321
3322
3323
3324
3325
3326
3327
3328
3329
3330
3331
3332
3333
3334
3335
3336
3337
3338
3339
3340
3341
3342
3343
3344
3345
3346
3347
3348
3349
3350
3351
3352
3353
3354
3355
3356
3357
3358
3359
3360
3361
3362
3363
3364
3365
3366
3367
3368
3369
3370
3371
3372
3373
3374
3375
3376
3377
3378
3379
3380
3381
3382
3383
3384
3385
3386
3387
3388
3389
3390
3391
3392
3393
3394
3395
3396
3397
3398
3399
3400
3401
3402
; SAVE ACTIVE PAGE VALUE
MOV ACTIVE_PAGE,AL
; SAVE SAVED LENGTH OF
MOV CX,CRT_LEN
; REGEN BUFFER
REGEN_BUFFER
; CONVENT AL WORD
CONV_AL_WORD
; DISPLAY PAGE VALUES
DISPLAY_PAGE_VALUES
; DISPLAY PAGE TIMES
DISPLAY_PAGE_TIMES
; SAVE START ADDRESS FOR
SAVE_START_ADDRESS
; LATER REQUIREMENTS
LATER_REQUIREMENTS
; START ADDRESS TO CX
START_ADDRESS_TO_CX
; DO NOT DIVIDE BY TWO
DO_NOT_DIVIDE_BY_TWO
; / Z FOR CRTC HANDLING
/ Z FOR CRTC HANDLING
; REG FOR START ADDRESS
REG_FOR_START_ADDRESS
; RECOVER PAGE VALUE
RECOVER_PAGE_VALUE
; GET CURSOR FOR THIS PAGE
GET_CURSOR_FOR_THIS_PAGE
; SET THE CURSOR POSITION
SET_THE_CURSOR_POSITION
; INCLUDE VSCROLL_IN
INCLUDE_VSCROLL_IN
; INCLUDE VSCROLL_OUT
INCLUDE_VSCROLL_OUT
; CHECK FOR SCROLL COUNT
CHECK_FOR_SCROLL_COUNT
; LOWER ROW
LOWER_ROW
; UPPER ROW
UPPER_ROW
; SAME AS REQUESTED
SAME_AS_REQUESTED
; YES, SET TO 0 FOR BLANK
YES_SET_TO_0_FOR_BLANK
; MOVE ROWS OF PELS UP
MOVE_ROWS_OF_PELS_UP
; SAVE DATA SEGMENT
SAVE_DATA_SEGMENT
; SET DATA SEGMENT
SET_DATA_SEGMENT
; SAVE MOVE COUNT
SAVE_MOVE_COUNT
; CLEAR HIGH BYTE
CLEAR_HIGH_BYTE
; SAVE POINTERS
SAVE_POINTERS
; MOVE THAT ROW
MOVE_THAT_ROW
; RECOVER POINTERS
RECOVER_POINTERS
; NEXT ROW
NEXT_ROW
; NEXT ROW COUNT
NEXT_ROW_COUNT
; DO MORE
DO_MORE
; RETURN TO CALLER
RETURN_TO_CALLER
; MOVE ROWS OF PELS DOWN
MOVE_ROWS_OF_PELS_DOWN
; SAVE DATA SEGMENT
SAVE_DATA_SEGMENT
; SET DATA SEGMENT
SET_DATA_SEGMENT
; SAVE MOVE COUNT
SAVE_MOVE_COUNT
; CLEAR HIGH BYTE
CLEAR_HIGH_BYTE
; SAVE POINTERS
SAVE_POINTERS
; MOVE THAT ROW
MOVE_THAT_ROW
; RECOVER POINTERS
RECOVER_POINTERS
; NEXT ROW
NEXT_ROW
; NEXT ROW COUNT
NEXT_ROW_COUNT
; DO MORE
DO_MORE
; RETURN TO CALLER
RETURN_TO_CALLER
; FILL ROW AFTER SCROLL
FILL_ROW_AFTER_SCROLL
; SEQUENCER
SEQUENCER
; MAP MASK
MAP_MASK
; ALL MAPS ON
ALL_MAPS_ON
; ZER0
ZER0
; COLUMN COUNT
COLUMN_COUNT
; SAVE PTR
SAVE_PTR
; SAVE PTR
SAVE_PTR
; RECOVER PTR
RECOVER_PTR
; GET COLOR VALUE
GET_COLOR_VALUE
; SEQUENCER
SEQUENCER
; MAP MASK
MAP_MASK
; SET THE COLOR
SET_THE_COLOR
; ALL BITS ON
ALL_BITS_ON
; COLUMN COUNT
COLUMN_COUNT
; TURN ON THOSE BITS IN
TURN_ON_THOSE_BITS_IN
1346 5F
1347 C3
1348 00
134B 86 03
134C B2 C9
134D BB 020F
134E 0015 R
1352 C3
1353 00
1354 1E
1355 E8 OCFF R
1357 8A F7
1359 2A FF
135A 52
135C 52
135D BB C3
135E BB 0485 R
1363 BB DB
1364 58
1366 58
1367 1F
1368 EB 131C R
136B 1E
136C EB OCFF R
136D 03 0E 04AA R
1373 4B
1374 4B
1375 8A F1
1377 EB 1348 R
137A C3
137B 1E
137C EB OCFF R
137F 8A F7
1383 52
1384 52
1387 EB 26 0485 R
138B BB DB
138C 58
138F 1F
1390 EB 131C R
1393 1E
1394 EB OCFF R
1395 03 0E 04AA R
1398 1F
139C 4B
139F 8A F1
13A2 C3
13A3
13A4 6A DB
13A5 6A 16EB R
13A8 80 FC 04
13AB 72 0E
13AD 0F 07
13B0 74 03
13B3 E9 1474 R
13B5 53
13B6 8A CI
13B8 EB 13F2 R
13BD 02 0D
13BF 8A E6
13C2 2A E3
13C3 00
13C7 EB 1432 R
13C9 8A 0F
13CB 03 FD
13CC 03 CC
13CD 75 F5
13CE 53
13CF 58
13D1 80 20
13D1 E8 1438 R
13D4 03 FD
13D6 75 F7
13DA EB OCFF R
13DD 80 3E 0499 R 07
13E0 00
13E1 E8 0465 R
13E3 BA 03DB
13E4 8A FE
13E5 E9 219E R
3403 C PCP DI ; ENABLED PLANES
3404 C RET ; RECOVER POINTER
3405 C END ; RETURN TO CALLER
3406 C PART_1 ENDP
3407 C
3408 C PROC NEAR
3409 C MOV DH, 3
340A C MOV DL, SEQ_ADDR ; SEQUENCER
340B C MOV AX, 020FH ; MAP ALL MAPS
340C C CALL OUT_DX ; ENABLE THE MAPS
340D C RET ; RETURN TO CALLER
340E C PART_2 ENDP
3415 C BLNK_K_3 PROC NEAR
3416 C PUSH DS ; BLANK FOR SCROLL UP
3417 C ASSUME DS:ABSO ; SAVE DATA SEGMENT
3418 C CALL DDS ; GET LOW MEMORY SEGMENT
3419 C PUSH DX ; ATTRIBUTE FOR BLANK LINE
341A C MOV BH, BH ; CLEAR HIGH BYTE
341B C SUB BH, BH ; SAVE
341C C PUSH AX ; SAVE BECAUSE OF MULTIPLY
341D C PUSH DX ; ROW COUNT
341E C PUSH AX ; CHARACTER HEIGHT
341F C POP BX, AX ; NET VALUE TO BX
3420 C POP AX ; RECOVER
3421 C POP DS ; SAVE DATA SEGMENT
3422 C S13: CALL PART_1 ; BLANK OUT ROW WITH COLOR
3423 C ASSUME DS:ABSO ; SAVE SEGMENT
3424 C PUSH DS ; LOW MEMORY SEGMENT
3425 C CALL DDS ; NEXT ROW
3426 C POP DS ; RECOVER
3427 C DEC BX ; NEXT
3428 C JNZ S13 ; DO MORE
3429 C CALL PART_2 ; RETURN TO CALLER
3430 C RET ; RETURN TO CALLER
3431 C BLNK_K_3 ENDP
3432 C BLNK_N_4 PROC NEAR
3433 C PUSH DS ; BLANK FOR SCROLL DOWN
3434 C ASSUME DS:ABSO ; SAVE DATA SEGMENT
3435 C CALL DDS ; GET LOW MEMORY SEGMENT
3436 C PUSH DX ; ATTRIBUTE FOR BLANK LINE
3437 C MOV BH, BH ; CLEAR HIGH BYTE
3438 C SUB BH, BH ; SAVE
3439 C PUSH AX ; SAVE BECAUSE OF MULTIPLY
343A C PUSH DX ; ROM POINTS
343B C MUL POINTS ; CHARACTER HEIGHT
343C C POP BX, AX ; NET VALUE TO BX
343D C POP AX ; RECOVER
343E C POP DS ; SAVE DATA SEGMENT
343F C S13_: ASSUME DS:NOTHING ; BLANK OUT ROW WITH COLOR
3440 C CALL PART_1 ; BLANK OUT ROW WITH COLOR
3441 C ASSUME DS:ABSO ; SAVE SEGMENT
3442 C PUSH DS ; LOW MEMORY SEGMENT
3443 C CALL DDS ; NEXT ROW
3444 C POP DS ; RECOVER
3445 C DEC BX ; NEXT
3446 C JNZ S13 ; DO MORE
3447 C CALL PART_2 ; RETURN TO CALLER
3448 C RET ; RETURN TO CALLER
3449 C BLNK_N_4 ENDP
3450 C
3451 C SCROLL_UP ; THIS ROUTINE MOVES A BLOCK OF CHARACTERS UP ON THE SCREEN
3452 C INPUT ; (AH) = CURRENT CRT MODE
3453 C ; (AL) = NUMBER OF ROWS TO SCROLL
3454 C ; (CX) = ROW/COLUMN OF UPPER LEFT CORNER
3455 C ; (DX) = ROW/COLUMN OF LOWER RIGHT CORNER
3456 C ; (BH) = ATTRIBUTE TO BE USED ON BLANKED LINE
3457 C ; (DS) = REGEN BUFFER SEGMENT
3458 C ; (ES) = REGEN BUFFER SEGMENT
3459 C OUTPUT ; NONE -- THE REGEN BUFFER IS MODIFIED
3460 C ASSUME CS:CODE, DS:ABSO, ES:NOTHING
3461 C SCROLL_UP PROC NEAR ; SAVE LINE COUNT IN BL
3462 C MOV BL, AL ; TEST FOR GRAPHICS MODE
3463 C CMP AH, 4 ; HANDLE SEPARATELY
3464 C JB N1 ; TEST FOR BM CARD
3465 C JE GRAPHICS_UP ; MOVE ONE ROW
3466 C N1: PUSH BX ; SAVE FILL PTR IN BP
3467 C PUSH AX ; UPPLEFT POSITION
3468 C CALL SCROLL_POSITION ; DO SETUP FOR SCROLL
3469 C PUSH AX ; FROM ADDRESS
3470 C ADD SI, AX ; # ROWS IN BLOCK
3471 C SUB AH, BL ; # ROWS TO BE MOVED
3472 C MOV BX, SI ; ROW_LOOP
3473 C MOV DI, BP ; COUNT LINES TO MOVE
3474 C NEXT_LINE_IN_BLOCK ; LOOP
3475 C POP AX ; CLEAR ENTRY
3476 C POP AX ; EFFECTS ATTRIB IN AH
3477 C MOV AL, ' ' ; FILL WITH BLANKS
3478 C CALL CLEAR_LINE ; CLEAR LINE
3479 C CALL CLEAR_LINE ; CLEAR LINE
3480 C CALL CLEAR_LINE ; CLEAR LINE
3481 C CALL CLEAR_LINE ; CLEAR LINE
3482 C CALL SCROLL_END ; SCROLL_END
3483 C M2: CALL N10 ; NEXT LINE IN BLOCK
3484 C ADD DI, BP ; COUNT LINES TO MOVE
3485 C DEC DI ; NEXT_LINE_IN_BLOCK
3486 C JNZ M2 ; LOOP
3487 C M3: POP AX ; CLEAR ENTRY
3488 C MOV AL, ' ' ; EFFECTS ATTRIB IN AH
3489 C CALL CLEAR_LINE ; CLEAR LINE
3490 C CALL CLEAR_LINE ; CLEAR LINE
3491 C CALL CLEAR_LINE ; CLEAR LINE
3492 C CALL CLEAR_LINE ; CLEAR LINE
3493 C CALL SCROLL_END ; SCROLL_END
3494 C Nh: CALL N11 ; NEXT LINE IN BLOCK
3495 C ADD DI, BP ; COUNT LINES TO MOVE
3496 C DEC DI ; NEXT_LINE_IN_BLOCK
3497 C JNZ Nh ; LOOP
3498 C M5: CALL DDS ; IS THIS THE BM CARD
3499 C CMP CRT_MODE, 7 ; SAVE/RESTORE BM/REGEN
3500 C JE N6 ; GET THE MODE SET
3501 C MOV AL, CRT_MODE_SET ; ALWAYS SET COLOR CARD
3502 C MOV DX, 0308H ; VIDEO_RET_HERE
3503 C OUT DX, AL ; VIDEO_RET_HERE
3504 C N6: JMP V_RET ; VIDEO_RET_HERE
13EE
13EF 8A DE
13F0 EB DC
13F2
13F2
13F2 F6 06 0487 P 04
13F7 74 12
13F9 52
13FA 86 03
13FC B2 DA
13FE 50
13FF
13FF EC
1400 8B 08
1402 74 FB
1404 B0 25
1406 8B D9
1408 EE
140A 38
140A SA
140B 8B 08
140D E8 1146 R
140E 03 06 044E R
1410 8B F8
1412 8B F0
1414 2B D1
1416 8B D1
1418 FE C2
141A 32 ED
141C 8B 044E R
141E 03 ED
1420 8B 044E R
1422 8B D1
1424 8B D1
1426 F6 26 044A R
1428 03 CO
142A 06
142C DF
142E 80 FB 00
1431 C3
1432
1432
1432 8A CA
1434 59
1435 57
1436 13/ A5
1438 57
1439 5E
143A C3
143B
143B
143B 8A CA
143D 57
143E F3/ AB
1440 5F
1441 C3
1442
1442
1442 FD
1443 8A D3
1445 8B 16EB R
1448 53
1449 c2
144B 8B F2 R
144E 74 20
1450 8B F0
1452 8A E6
1454 2A E3
1456
1456 E8 1432 R
1459 2B D1
145B FE CC
145D 75
145F 75
1461 54
1462 80 20
1464 E8 143B R
1467 2B FD
1469 FE CB
146B 75 77
146D E9 13DA 0
1470 8A DE
1472 8A DE
1474 EB ED
3529 C N7: MOV BL,DH ; BL = BLANK FIELD
3530 C JMP M3 ; GET ROW COUNT
3531 C SCROLL_UP ENDP
3533 C
3534 C ------ HANDLE COMMON SCROLL SET UP HERE
3535 C
3536 C SCROLL_POSITION PROC NEAR
3537 C TEST INFO,4
3538 C JZ N9
3539 C
3540 C ------ 80X25 COLOR CARD SCROLL
3541 C PUSH DX
3542 C PUSH AX
3543 C PUSH BX
3544 C PUSH DI
3545 C PUSH BP
3546 C PUSH DS
3547 C IN AL,DX ; AL = ATTR
3548 C TEST AL,8 ; WAIT FOR VERT RETRACE
3549 C ZF ; WAIT FOR VERT RETRACE
3550 C MOV DI,25H ; WAIT DISP_ENABLE
3551 C MOV AH,0DH ; TURN OFF VIDEO
3552 C OUT DX,AL ; DURING VERTICAL RETRACE
3553 C POP DS
3554 C POP DX
3555 C CALL POSITION ; CONVERT TO REGEN POINTER
3556 C ADD AX,CRT_START ; OFFSET OF ACTIVE PAGE
3557 C MOV DI,AH ; ADDRESS FOR SCROLL
3558 C MOV SI,AX ; FROM ADDRESS FOR SCROLL
3559 C SUB DX,CX ; DX=ROWS , COLS
3560 C INC DL ; INCREMENT FOR O ORIGIN
3561 C XOR CH,CH ; ZERO HIGH BYTE OF COUNT
3562 C MOV CRT_COLS,CH ; PUSH CURRENT COLS
3563 C ADD BP,BP ; TIMES 2 FOR ATRR BYTE
3564 C MOV BL,DL ; GET LINE COUNT
3565 C MUL BYTE PTR CRT_COLS ; OFFSET TO START ADDRESS
3566 C ADD AX,AH ; #2 FOR ATTRIBUTE BYTE
3567 C PUSH DS ; ESTABLISH POINTING
3568 C CMP BL,0 ; FOR BOTH POINTERS
3569 C JNZ N9 ; O MEANS BLANK FIELD
3570 C POP DS ; RETURN WITH FLAGS SET
3571 C SCROLL_POSITION ENDP
3572 C
3573 C ------ MOVE_ROW
3574 C
3575 C N10 PROC NEAR ; GET # OF COLS TO MOVE
3576 C MOV CL,DL ; SAVE START ADDRESS
3577 C PUSH SI ; MOVE THAT LINE ON SCREEN
3578 C PUSH DI ; RECOVER ADDRESSES
3579 C REP MOVSB ; CLEAR_ROW
3580 C POP DI ; RECOVER ADDRESSES
3581 C POP SI ; RECOVER ADDRESSES
3582 C ENDN
3583 C
3584 C ------ CLEAR_ROW
3585 C
3586 C N11 PROC NEAR ; GET # COLUMNS TO CLEAR
3587 C MOV CL,DL ; STORE THE FILL CHARACTER
3588 C PUSH DI ; STORE THE FILL CHARACTER
3589 C REP STOSB ; STORE THE FILL CHARACTER
3590 C POP DI ; STORE THE FILL CHARACTER
3591 C RET
3592 C ENDN
3593 C
3594 C ------ SCROLL_DOWN
3595 C THIS ROUTINE MOVES THE CHARACTERS WITHIN A
3596 C DEFINED AREA DOWN ON THE SCREEN, FILLING THE
3597 C TOP LINES WITH A DEFAULT CHARACTER
3598 C INPUT:
3599 C (AH) = CURRENT CRT MODE
3600 C (AL) = NUMBER OF LINES TO SCROLL
3601 C (CX) = UPPER LEFT CORNER OF REGION
3602 C (DX) = LOWER RIGHT CORNER OF REGION
3603 C (BR) = FILL CHARACTER
3604 C (DS) = DATA SEGMENT
3605 C (ES) = REGEN SEGMENT
3606 C OUTPUT:
3607 C NONE -- SCREEN IS SCROLLED
3608 C
3609 C SCROLL_DOWN PROC NEAR
3610 C STD ; SCROLL DOWN
3611 C MOV BL,AL ; LINE COUNT TO BL
3612 C CALL SCROLL_POSITION ; SAVE ATTRIBUTE IN BH
3613 C PUSH BX ; LOWER RIGHT CORNER
3614 C CALL SCROLL_POSITION ; GET REGEN LOCATION
3615 C SUB AX,AH ; S1 IS FROM ADDRESS
3616 C SUB AH,AL ; COUNT TO MOVE IN SCROLL
3617 C MOV AH,0 ; MOVE ONE ROW
3618 C POP AX ; RECOVER ATTRIBUTE IN AH
3619 C MOV AH,1 ; CLEAR ONE ROW
3620 C CALL N11 ; GO TO NEXT ROW
3621 C DEC AH ; SCROLL_END
3622 C JNZ N13
3623 C
3624 C N13: CALL N10 ; MOVE ONE ROW
3625 C SUB SI,BP ; SCROLL_DOWN
3626 C DEC AH ; SCROLL_DOWN
3627 C JNZ N14 ; SCROLL_DOWN
3628 C
3629 C N14: POP AX ; RECOVER ATTRIBUTE IN AH
3630 C MOV AH,1 ; CLEAR ONE ROW
3631 C CALL N11 ; GO TO NEXT ROW
3632 C DEC AH ; SCROLL_END
3633 C JNZ N15 ; SCROLL_END
3634 C
3635 C N15: MOV BL,DH ; BL = BLANK FIELD
3636 C JMP N14 ; SCROLL_DOWN
3637 C SCROLL_DOWN ENDP
3638 C
3639 C ------ SCROLL_UP
3640 C THIS ROUTINE SCROLLS UP THE INFORMATION ON THE CRT
3641 C ENTRY:
3642 C CH,CL = UPPER LEFT CORNER OF REGION TO SCROLL
3643 C DH,DL = LOWER RIGHT CORNER OF REGION TO SCROLL
3644 C BR = FILL VALUE FOR BLANKED LINES
3645 C AL = # LINES TO SCROLL (AL=0 MEANS BLANK THE ENTIRE
3646 C FIELD)
3647 C DS = DATA SEGMENT
3655    C    ES = REGEN SEGMENT
3656    C
3657    C    NOTHING, THE SCREEN IS SCROLLED
3658    C
3659    C    GRAPHICS_UP PROC NEAR
3660    C    SAVE LINE COUNT IN BL
3661    C    GET UPPER LEFT POSITION INTO AX REG
3662    C
3663    C    ---- USE CHARACTER SUBROUTINE FOR POSITIONING
3664    C    ---- ADDRESS RETURNED IS MULTIPLIED BY 2 FROM CORRECT VALUE
3665    C
3666    C    CALL GRAPH_POSN
3667    C    MOV D1,AX
3668    C    ; SAVE RESULT AS
3669    C    ; DESTINATION ADDRESS
3670    C
3671    C    ---- DETERMINE SIZE OF WINDOW
3672    C
3673    C    SUB DX,CX
3674    C    ADD DX,101H
3675    C    SAL DH,1
3676    C    SAL DH,1
3677    C    SAL DH,1
3678    C
3679    C    ---- DETERMINE CRT MODE
3680    C
3681    C    CMP CRT_MODE,6
3682    C    JNC R7
3683    C    ; TEST FOR MEDIUM RES
3684    C    ; FIND_SOURCE
3685    C
3686    C    ---- MEDIUM RES UP
3687    C    SAL DL,1
3688    C    ; SINCE 2 BYTES/CHAR
3689    C
3690    C    ;---- DETERMINE THE SOURCE ADDRESS IN THE BUFFER
3691    C    R7:
3692    C    PUSH DS
3693    C    POP DS
3694    C    SUB CH,CH
3695    C    SAL BL,1
3696    C    JB RI1
3697    C    MOV AL,BL
3698    C    MOV AH,80
3699    C    MUL AH
3700    C    MOV SI,D1
3701    C    ADD SI,AX
3702    C    ADD SI,AX
3703    C    ADD SI,AX
3704    C    SUB SI,AL
3705    C
3706    C    ;---- LOOP THROUGH, MOVING ONE ROW AT A TIME, BOTH EVEN AND ODD FIELDS
3707    C
3708    C    R8:
3709    C    CALL RI7
3710    C    SUB D1,2000H-B0
3711    C    DEZ AH
3712    C    JNZ R8
3713    C    ; NUMBER OF ROWS TO MOVE
3714    C    ; CONTINUE TILL ALL MOVED
3715    C
3716    C    ;---- FILL IN THE VACATED LINE(S)
3717    C
3718    C    R9:
3719    C    MOV AL,BH
3720    C    CALL RI8
3721    C    SUB D1,2000H-B0
3722    C    DIS BL
3723    C    JNC RI1
3724    C    JMP RI_RET
3725    C
3726    C    ;---- ROUTINE TO MOVE ONE ROW OF INFORMATION
3727    C
3728    C    R10:
3729    C    GRAPHICS_UP ENDP
3730    C
3731    C    ;---- ROUTINE TO MOVE ONE ROW OF INFORMATION
3732    C
3733    C    R17 PROC NEAR
3734    C    MOV CL,DL
3735    C    PUSH SI
3736    C    PUSH DI
3737    C    REP MOVSB
3738    C    POP SI
3739    C    POP DI
3740    C    ADD D1,2000H
3741    C    PUSH SI
3742    C    PUSH DI
3743    C    REP MOVSB
3744    C    POP SI
3745    C    POP DI
3746    C    RET
3747    C    END
3748    C
3749    C    ;---- CLEAR A SINGLE ROW
3750    C
3751    C    R18 PROC NEAR
3752    C    ; NUMBER OF BYTES IN FIELD
3753    C    ; SAVE POINTER
3754    C    ; STORE THE NEW VALUE
3755    C    ; POINT BACK
3756    C    ; POINT TO ODD FIELD
3757    C    ; FILL THE ODD FIELD
3758    C    ; RETURN TO CALLER
3759    C
3760    C    MEM_OET PROC NEAR
3761    C    ; NUMBER OF BYTES IN FIELD
3762    C    ; SAVE POINTER
3763    C    ; STORE THE NEW VALUE
3764    C    ; POINT BACK
3765    C    ; POINT TO ODD FIELD
3766    C    ; FILL THE ODD FIELD
3767    C    ; RETURN TO CALLER
3768    C
3769    C    MEM_OET PROC NEAR
3770    C    CALL RI8
3771    C    MOV AH,INFO
3772    C    AND AH,000H
3773    C    POP DS
3774    C    POP AX
3775    C    STC
3776    C    INT N
3777    C    RET
3778    C
3779    C    MIN:
3780    C    CLC
3781    C    RET
1508
150B
150B
150B E9 13A3 R
150E
150E E8 12D1 R
1511 8A 26 0449 R
1515 80 0511
1518 76 F1
151A 80 FC 0D
151D 80 FC 07
151F E9 219E R
1522 BA A000
1525 80 0511
1528 80 FC OF
152B 76 0F
152D 80 FF F7 R
1530 73 03
1532 80 0501
1535 C3
1536 80 0501
1536 80 0501
1537 EB 1522 R
153A 8C C2
153C 5A
153D 8A D8
1541 53
1542 8A 3E 0H62 R
1544 8C EC R
1549 5B
154A 8B D8
154C 8B D8
154E 8B D8
1550 8A C3
1552 8A C3
1554 8A C3
1556 8A C3
1558 8A C3
155A 8A C3
155C 8A C3
155E 8A C3
1560 8A C3
1562 8A C3
1564 8A C3
1566 8A C3
1568 8A C3
156A 8A C3
156C 8A C3
156E 8A C3
1570 1E
1571 EB 0CFE R
1575 50
1576 8B C1
1577 8B C1
1578 8B C1
1579 8B C1
157A 8B C1
157B 8B C1
157C 8B C1
157D 8B C1
157E 8B C1
157F 8B C1
1580 1F
1581 52
1582 8B C5
1584 8B 03
1586 8B CE
1587 8B 0D15 R
1588 8B CA
1589 8B 0D15 R
1590 8B 0D15 R
1593 5A
1594 8B 12E0 R
1597 52
1598 8B C5
1599 8B 03
159A 8B CE
159B 8B 0D15 R
159C 8B CA
159D 8B 0D15 R
159E 8B 0D15 R
15A3 E8 1353 R
15A4 E9 219E R
15A6 8A DF
15A8 EB F6
15AD 8A DF
15AE E9 1442 R
15AD E9 1442 R
15B0
15B0 E8 12D1 R
15B3 8A 26 0449 R
15B7 80 0511
15BA 76 F1
15BB 80 FC 0D
15BF 80 FC 07
15C1 80 FC 0D
15C4 76 0C
15C6 80 FC 06
15C9 80 FC 07
15CB 80 FC 07
15CD 80 FC 07
15CF E9 219E R
15D2 FD
15D2 FD
3781 C MEM_DET ENDP
3782 C
3783 C ------ SCROLL ACTIVE PAGE UP
3784 C
3785 C SC_2:
3786 C JMP SCROLL_UP
3787 C
3788 C AHI:
3789 C ASSUME DS:ABSO
3790 C CALL FLT_A
3791 C MOV AH,CRT_MODE
3792 C CMP AH,0DH ; GET CURRENT MODE
3793 C JBE SC_2 ; ANY OF THE OLD MODES
3794 C CMP AH,0FH ; NEW GRAPHICS MODES
3795 C JAE GRAPHICS_UP_2 ; NOT A RECOGNIZED MODE
3796 C JMP V_RET
3797 C
3798 C GR_ST_1 PROC NEAR
3799 C MOV DX,0A000H ; REGEN BUFFER
379A C MOV AH,0BH ; GRAPHICS WRITE MODE
379B C INT 10H ; GET CURRENT MODE
379C C CM7 AH,0FH ; GRAPHICS WRITE MODE
379D C JB VV1 ; ANY OF THE OLD MODES
379E C CALL MEM_DET ; NEW GRAPHICS MODES
379F C JNC VV1 ; NOT A RECOGNIZED MODE
37A0 C MOV BP,0501H ; GRAPHICS WRITE MODE
37A1 C
37A2 C VV1: REF
37A3 C GR_ST_1 ENDP
37A4 C
37A5 C GRAPHICS_UP_2 PROC NEAR
37A6 C ASSUME DS:ABSO
37A7 C PUSH DX
37A8 C CALL GR_ST_1 ; SET SEGMENT, WRITE MODE
37A9 C SRLOAD ES
37AA C MOV ES,DX
37AB C PUSH DX
37AC C PUSH AX
37AD C PUSH BX
37AE C PUSH DI
37AF C PUSH SI
37B0 C PUSH BP
37B1 C PUSH DS
37B2 C PUSH CX
37B3 C PUSH DX
37B4 C PUSH AX
37B5 C PUSH BX
37B6 C PUSH DI
37B7 C PUSH SI
37B8 C PUSH BP
37B9 C PUSH DS
37BA C PUSH CX
37BB C PUSH DX
37BC C PUSH AX
37BD C PUSH BX
37BE C PUSH DI
37BF C PUSH SI
37C0 C PUSH BP
37C1 C PUSH DS
37C2 C PUSH CX
37C3 C PUSH DX
37C4 C PUSH AX
37C5 C PUSH BX
37C6 C PUSH DI
37C7 C PUSH SI
37C8 C PUSH BP
37C9 C PUSH DS
37CA C PUSH CX
37CB C PUSH DX
37CC C PUSH AX
37CD C PUSH BX
37CE C PUSH DI
37CF C PUSH SI
37D0 C PUSH BP
37D1 C PUSH DS
37D2 C PUSH CX
37D3 C PUSH DX
37D4 C PUSH AX
37D5 C PUSH BX
37D6 C PUSH DI
37D7 C PUSH SI
37D8 C PUSH BP
37D9 C PUSH DS
37DA C PUSH CX
37DB C PUSH DX
37DC C PUSH AX
37DD C PUSH BX
37DE C PUSH DI
37DF C PUSH SI
37E0 C PUSH BP
37E1 C PUSH DS
37E2 C PUSH CX
37E3 C PUSH DX
37E4 C PUSH AX
37E5 C PUSH BX
37E6 C PUSH DI
37E7 C PUSH SI
37E8 C PUSH BP
37E9 C PUSH DS
37EA C PUSH CX
37EB C PUSH DX
37EC C PUSH AX
37ED C PUSH BX
37EE C PUSH DI
37EF C PUSH SI
37F0 C PUSH BP
37F1 C PUSH DS
37F2 C PUSH CX
37F3 C PUSH DX
37F4 C PUSH AX
37F5 C PUSH BX
37F6 C PUSH DI
37F7 C PUSH SI
37F8 C PUSH BP
37F9 C PUSH DS
37FA C PUSH CX
37FB C PUSH DX
37FC C PUSH AX
37FD C PUSH BX
37FE C PUSH DI
37FF C PUSH SI
3800 C ASSUME DS:NOTHING
3801 C PUSH DS
3802 C POP DS
3803 C POP DS
3804 C POP DS
3805 C POP DS
3806 C POP DS
3807 C POP DS
3808 C POP DS
3809 C POP DS
380A C POP DS
380B C POP DS
380C C POP DS
380D C POP DS
380E C POP DS
380F C POP DS
3810 C ASSUME DS:NOTHING
3811 C PUSH DS
3812 C POP DS
3813 C POP DS
3814 C POP DS
3815 C POP DS
3816 C POP DS
3817 C POP DS
3818 C POP DS
3819 C POP DS
381A C POP DS
381B C POP DS
381C C POP DS
381D C POP DS
381E C POP DS
381F C POP DS
3820 C ASSUME DS:NOTHING
3821 C PUSH DS
3822 C POP DS
3823 C POP DS
3824 C POP DS
3825 C POP DS
3826 C POP DS
3827 C POP DS
3828 C POP DS
3829 C POP DS
382A C POP DS
382B C POP DS
382C C POP DS
382D C POP DS
382E C POP DS
382F C POP DS
3830 C ASSUME DS:NOTHING
3831 C PUSH DS
3832 C POP DS
3833 C POP DS
3834 C POP DS
3835 C POP DS
3836 C POP DS
3837 C POP DS
3838 C POP DS
3839 C POP DS
383A C POP DS
383B C POP DS
383C C POP DS
383D C POP DS
383E C POP DS
383F C POP DS
3840 C ASSUME DS:NOTHING
3841 C PUSH DS
3842 C POP DS
3843 C POP DS
3844 C POP DS
3845 C POP DS
3846 C POP DS
3847 C POP DS
3848 C POP DS
3849 C POP DS
384A C POP DS
384B C POP DS
384C C POP DS
384D C POP DS
384E C POP DS
384F C POP DS
3850 C ASSUME DS:NOTHING
3851 C PUSH DS
3852 C POP DS
3853 C POP DS
3854 C POP DS
3855 C POP DS
3856 C POP DS
3857 C POP DS
3858 C POP DS
3859 C POP DS
385A C POP DS
385B C POP DS
385C C POP DS
385D C POP DS
385E C POP DS
385F C POP DS
3860 C ASSUME DS:NOTHING
3861 C PUSH DS
3862 C POP DS
3863 C POP DS
3864 C POP DS
3865 C POP DS
3866 C POP DS
3867 C POP DS
3868 C POP DS
3869 C POP DS
386A C POP DS
386B C POP DS
386C C POP DS
386D C POP DS
386E C POP DS
386F C POP DS
3870 C ASSUME DS:NOTHING
3871 C PUSH DS
3872 C POP DS
3873 C POP DS
3874 C POP DS
3875 C POP DS
3876 C POP DS
3877 C POP DS
3878 C POP DS
3879 C POP DS
387A C POP DS
387B C POP DS
387C C POP DS
387D C POP DS
387E C POP DS
387F C POP DS
3880 C ASSUME DS:NOTHING
3881 C PUSH DS
3882 C POP DS
3883 C POP DS
3884 C POP DS
3885 C POP DS
3886 C POP DS
3887 C POP DS
3888 C POP DS
3889 C POP DS
388A C POP DS
388B C POP DS
388C C POP DS
388D C POP DS
388E C POP DS
388F C POP DS
3890 C ASSUME DS:NOTHING
3891 C PUSH DS
3892 C POP DS
3893 C POP DS
3894 C POP DS
3895 C POP DS
3896 C POP DS
3897 C POP DS
3898 C POP DS
3899 C POP DS
389A C POP DS
389B C POP DS
389C C POP DS
389D C POP DS
389E C POP DS
389F C POP DS
38A0 C ASSUME DS:NOTHING
38A1 C PUSH DS
38A2 C POP DS
38A3 C POP DS
38A4 C POP DS
38A5 C POP DS
38A6 C POP DS
38A7 C POP DS
38A8 C POP DS
38A9 C POP DS
38AA C POP DS
38AB C POP DS
38AC C POP DS
38AD C POP DS
38AE C POP DS
38AF C POP DS
38B0 C ASSUME DS:NOTHING
38B1 C PUSH DS
38B2 C POP DS
38B3 C POP DS
38B4 C POP DS
38B5 C POP DS
38B6 C POP DS
38B7 C POP DS
38B8 C POP DS
38B9 C POP DS
38BA C POP DS
38BB C POP DS
38BC C POP DS
38BD C POP DS
38BE C POP DS
38BF C POP DS
38C0 C ASSUME DS:NOTHING
38C1 C PUSH DS
38C2 C POP DS
38C3 C POP DS
38C4 C POP DS
38C5 C POP DS
38C6 C POP DS
38C7 C POP DS
38C8 C POP DS
38C9 C POP DS
38CA C POP DS
38CB C POP DS
38CC C POP DS
38CD C POP DS
38CE C POP DS
38CF C POP DS
38D0 C ASSUME DS:NOTHING
38D1 C PUSH DS
38D2 C POP DS
38D3 C POP DS
38D4 C POP DS
38D5 C POP DS
38D6 C POP DS
38D7 C POP DS
38D8 C POP DS
38D9 C POP DS
38DA C POP DS
38DB C POP DS
38DC C POP DS
38DD C POP DS
38DE C POP DS
38DF C POP DS
38E0 C ASSUME DS:NOTHING
38E1 C PUSH DS
38E2 C POP DS
38E3 C POP DS
38E4 C POP DS
38E5 C POP DS
38E6 C POP DS
38E7 C POP DS
38E8 C POP DS
38E9 C POP DS
38EA C POP DS
38EB C POP DS
38EC C POP DS
38ED C POP DS
38EE C POP DS
38EF C POP DS
38F0 C ASSUME DS:NOTHING
38F1 C PUSH DS
38F2 C POP DS
38F3 C POP DS
38F4 C POP DS
38F5 C POP DS
38F6 C POP DS
38F7 C POP DS
38F8 C POP DS
38F9 C POP DS
38FA C POP DS
38FB C POP DS
38FC C POP DS
38FD C POP DS
38FE C POP DS
38FF C POP DS
3900 C MEM_DET ENDP
3901 C GRAPHICS_DN_2 PROC NEAR
3902 C STD
1503  BA DB ; LINE COUNT
1504 DA  ; SAVE LOWER RIGHT
1506 B8 1522 R ; REGEN SEGMENT
1509 BB C2
150B 5A
150E DD E2 ; ADJUST COUNT
1510 8D 
1511 4F C0
1515 F0  ; MOV CHAR ROW UP BY ONE
1516 08 ; MOV LEFT_ASCII_PAGE
1517 C3  ; AGGRESS IN REGEN
1518 D0 F8 ; SSH CRUD_COLS
1519 34  ; ADD CURT_COLS
151A 33 C0 ; ADJUST_COUNT
151C 59 ; USE_AH
151D C1 ; MOVE_SUB_R ION 
151E 51 ; CPUs_OE
151F F7 EF ; BYTES_PER_ROW
1520 FF E7 ; BYTES_PER_CHAR
1521 F9 FF ; BYTE_ROUND
1525 CD ; ASSUME_DS NOTHING
1526 CD ; SET_DS TO THE_REGEN_SEGMENT segment 
1527 AF DF A5 ; SCROLL window 
1529 90 ; ADJUSTib_ : 
152A DD C9 ; BLANK ENTIRE窗口
152C 03 ; ASSAME_DS NOT
AREA  REG    CS
1530 F6 ; SIZE_FILL secs
1531 89  ; POP AX
1532 DEC ; PUSH ES
1533 6A  ; PUSH DX
1534 52  ; CALL graph add range graphics window
1535 CD B ; AdjustableStartADDR
1536 00 ; CALL c/AD4_
1537 DD ; Start tab l ,0 OK  set seq enabled
1538 F7 ; graphs_out_dx_Xh
1539 C8 ; OUT_DN ADDR hop
C3
153B D0 D0 ;_grapho_verify
153D CD 5  ; GRAPHICS
153E 59 ; set = begin
153F 30 ; PPQ
1540 42 DE ; crck_4
1542 52
1543 DD F6 ; graph d nv v_
1544 8B  ; xJUM_ smooth
1545 CD ; MSG_IDENT
HOME. IN \$_PROG ;
SUBTL VGRW; INC
PACE
NAME;
ASSUME DS:ACCESS
BEGIN PROG. NEAR: IDENTIFAXM  DIV_SLOW 
ENDL  ACC -DS
END Hay, DELMER P521 USE, W
ENTRY j départ_
MATE_HAS_COLOR_TBD ( AL colorful, i
TO_
POWER =
END この POLZEJOC B ECTE LIKE IND deadline1 st prompt ( return to power )
ENTRY Beluez_COLOR_EOPLP_ COLOR_FLAGS
YAN  :COLOR UTILIYS
S社会保障条鞭;//SAN; Color_ charger
IDENTIDEXH ));
PROC NEAR=I _IDENTIF.; =SPECISE-2� ; Prelude th entry_progrma next S3Female PREAFO  SUSP+PREMZ1;
   _;BUILD_GETMA MICPFARS::r, _POLISTSUE );
   BEL = Open , no type;
IF (cuadicx-supiedaround)
(
YYYXB  MVC ; ADD_ADDR = START_ADDR
CYRDB TYPE
PCEN ass
PCPPE _POLITURAL LEO IMPACTOF cx';i ;I ASSIGNAL hız  ANLP HC
;PL Associate _PER_deep-open
PCCLOSE WIWRSHLIRE ;

BS
DA W
ULATION(
BUILD ANDING CLEAR_(CLR-ADDR)
EXPAND_COUNT + BYTE ;

FA ARR CH_CENTERعباد، ;Prob = LAAK-RI p-fälle_new=PUBL_const
ICI tiけの %__(
RESHA YEDIT; 8 FORWARDできた。
CIS ;L-位的rep:
FELL CITY_DEEP;
SOH restore
C
;
END:
Building 탑 dragged allowed to be specified influencing new ;
EXIT(Continue, quits to code-P move to next SS!
RESHA RT;
; Eating" Mercantile PRIVATE ACCESS
    
END
16 BITS. THE RESULT IS LEFT IN AX

S21
PUSH REAR ; SAVE REGISTERS
PUSH CX
PUSH DX
SUB DX, DX ; RESULT REGISTER
MOV CX, 1 ; MASK REGISTER

S22:
MOV BX, AX
ARD BX, CX
SHL BX, 1
MOV AX, BX
AND BX, CX
SHL BX, 1
MOV AX, BX
AND BX, CX
SHL BX, 1
JNC S22
POP AX, DX
POP BX
POP CX
POP DX
RET
ENDP

S21
PROC NEAR
GRAPH_POSN LABEL NEAR ; GET CURRENT CURSOR
PUSH BX
PUSH DX
MOV BX, AX
MOV AL, AH
MUL PTR CRT_COLS
SHL AX, 1
ADD BX, BH
ADD AX, BX
POP BX
POP DX
RET
ENDP

S26
PROC NEAR
GRAPH_POSN LABEL NEAR ; GET CURRENT CURSOR
PUSH BX
PUSH DX
MOV BX, AX
MOV AL, AH
MUL PTR CRT_COLS
SHL AX, 1
ADD BX, BH
ADD AX, BX
ISOLATE COLUMN VALUE
DETERMINE OFFSET
RECOVER POINTER
POP BX
POP DX
RET
ENDP

CUR_ENTY
BH = DISPLAY PAGE
EXIT
AX = CURSOR POSITION FOR REQUESTED PAGE

CUR_ENTY
ASSUME DS:ABS0
PUSH BX
PUSH DX
MOV BL, BH ; SAVE REGISTER
GET TO LOW BYTE
SUB BH, BL ; ZERO HIGH BYTE
FOR ROW COUNT
CURSOR REQUESTED PAGE
RECOVER REGISTER
POP BX
POP DX
RET

GRX_PSN
ENTRY
AX = CURSOR POSITION IN DESIRED PAGE
BH = DESIRED PAGE
EXIT
AX = BYTE OFFSET INTO REGEN

GRX_PSN PROC NEAR
PUSH BX
PUSH DX
SUB CH, CH ; SAVE
ZERO
MOV BX, AX ; ROW, COLUMN
MOV AL, AH ; ROW, COLUMN
MUL PTR CRT_COLS ; ROW * COLUMNS/BW
ADD BX, BH ; BYTES PER ROW
ADD BX, BL ; ADD IN COLUMN
MOV BX, CRT_LEN ; PAGE LENGTH
POP DX
POP BX
RET
ENDP

GP_3:
ADD AX, BX ; ADD IN THE PAGE LENGTH
LOOP GP_3 ; DO FOR NUMBER OF PAGES

GP_2:
POP DX ; RECOVER
POP CX ; RECOVER
POP BX ; RECOVER
RET

GRX_PSN ENDP

MK_ES:
MOV SI, OB000H ; SAVE
MOV DI, EQUI_FLAG ; SAVE
AND DI, 030H ; SAVE
CMP DI, 040H ; SAVE
JNE FS_A ; SAVE
MOV SI, OB000H ; SAVE

P6_A:
MOV ES, SI ; RECOVER
RET

READ_AC_CURRENT
THIS ROUTINE READS THE ATTRIBUTE AND CHARACTER AT THE CURRENT CURSOR POSITION AND RETURNS THEM TO THE CALLER
INPUT
(AH) = CURRENT CRT MODE
(BH) = DISPLAY PAGE (ALPHA MODES ONLY)
(DS) = DATA SEGMENT
(CX) = CURSOR POSITION
OUTPUT
(AL) = CHAR READ
(AH) = ATTRIBUTE READ

READ_AC_CURRENT PROC NEAR
CALL F15 ; FIND POSITION
MOV SI, BX ; ADDRESSING IN SI
MOV DX, ADDR_6845 ; GET BASE ADDRESS
ADD DX, 6 ; POINT AT STATUS PORT
TEST INFO, 4
1715 06
1716 1F
1717 74 0B

4159 PUSH ES
4160 POP DS ; SEGMENT FOR QUICK ACCESS
4161 JZ P3A

;------WAIT FOR HORIZONTAL RETRACE

1719 EC
171A A8 01
171B 74 FB
171C FA
171D 74 01
171E EC
1720 A8 01
1721 74 FB
1722 74 01
1723 AG
1724 74 191E R
1725 74 191E R
1726 74 191E R
1727 74 191E R
1728 READ_AC_CURRENT EROM

P2:
4166 IN AL, DX
4167 TEST AL, 1
4168 JNZ P2
4169 CLI
4170 P3:
4171 IN AL, DX
4172 TEST AL, 1
4173 JZ P3
4174 LODSW
4175 MOV V, RT
4176 JMP V, RT
4177 ; GET THE CHAR/ATTR

MED_READ_BYTE:
; ROUTINE WILL TAKE 2 BYTES FROM THE REGEN BUFFER. COMPARE AGAINST THE CURRENT FOREGROUND COLOR AND PLACE THE CORRESPONDING ON/OFF BIT PATTERN (WITH THE CURRENT POSITION IN THE SAVE AREA
; ENTRY
S1_DS = POINTER TO REGEN AREA OF INTEREST
S1_AX = CURRENT FOREGROUND COLOR
SP = POINTER TO SAVE AREA
; EXIT
BP IS INCREMENT AFTER SAVE

1729 8A 24
172A 8A 44 01
172D B9 C000
1730 B2 00
1731 8B 00
1732 85 C1
1733 F8
1734 73 01
1735 75 15
1736 00 D2
1737 D1 E9
1738 D1 E9
1739 73 F2
1740 88 56 06
1743 85 C3
1745 8B EC
1746 E3 16EB R
1748 E6 16AB R
174B 88 FO
174D 83 EC 08
1750 8B EC
1752 80 3E 0449 R 06
1755 86 06
1756 75 1A
1759 72 1A

S23 PROC NEAR
4194 MOV [SI+1]
4195 MOV AL,[SI+1]
4196 MOV CX,0000H
4197 MOV DL,0
4198 S24:
4199 TEST AX,CX
419A CLC
419B JZ S25
419C STC
419D MOV [BP],DL
419E INC BP
419F RET
4200 ENDP

GRAPHICS_READ PROC NEAR
4219 CALL MK_ES
4220 CALL CAL_S
4221 MOV S1,AX
4222 SUB SP,8
4223 MOV BP,SP
4224 MOV BP,SP
4225 S22:
4226 CMP CRT_MODE,6
4227 JNC S23
4228 PUSH ES
4229 PUSH DS
4230 POP DS
4231 JC S13P
4232 ; POINT TO REGEN SEGMENT
4233 ; MEDIUM RESOLUTION
4234 ;---- DETERMINE GRAPHICS MODES
4235 ;---- HIGH RESOLUTION READ
4236 ;---- GET VALUES FROM REGEN BUFFER AND CONVERT TO CODE POINT
4237 ;---- MEDIUM RESOLUTION READ
4238 MOV Dh,h
4239 S12P:
4240 MOV AL,[SI]
4241 MOV [BP],AL
4242 INC BP
4243 MOV AL,[SI+200H]
4244 MOV [BP],AL
4245 INC BP
4246 ADD SI,80
4247 DEC DH
4248 JNZ S12P
4249 JMP S15P
4250 ; POINTS
4251 ;---- MEDIUM RES READ
4252 S13P:
4253 SAL SI,1
4254 MOV DH,h
4255 S14P:
4256 CALL S23
4257 ; GET PAIR BYTES
4258 ; INTO SINGLE BYTE
4259 ; GO TO NEXT REGION
4260 ; GET THIS PAIR INTO SAVE
4261 ; ADJUST POINTER BACK INTO
4262 ; LOOP
4263 ; KEEP GOING UNTIL 8 DONE
4264 ;---- SAVE AREA HAS CHARACTER IN IT, MATCH 17
4265 ;---- FIND_CHAR
4266 ;---- ESTABLISH ADDRESSING
4267 ;---- BEGINNING OF SAVE AREA
4268 ;---- ENSURE DIRECTION
4269 ;---- CURRENT CODE POINT BEING
4270 ;---- MATCHED
4271 ;---- ADDRESSING TO STACK
4272 ;---- FOR THE STRING COMPARE
4273 ;---- NUMBER TO TEST NEXT
4274 ;---- SAVE SAVE AREA POINTER
4275 ;---- SAVE CODE POINTER
17A3 B9 0068  4285 C   MOV   CX, 8           ; NUMBER OF BYTES TO MATCH
17A6 F3 / A   4286             REPE   CMSB      ; COMPARE THE 8 BYTES
17A8 5F        4287            POP    DI         ; RECOVER THE POINTERS
17A9 5E        4288            POP    BP         ; OR
17AA 74 1D     4289            JZ     S18P       ; IF ZERO FLAG SET,
17AC FE D0     4290              INC    AL          THEN MATCH OCCURRED
17AE 83 C7 08  4291            ADD    DI, 8          NEXT CODE POINT
17B1 75 ED     4292            DEC    DX          NOT TO NEXT
17B2 75 E8     4293            JNZ    S17P         LAST CONTROL
17B4 30 C0     4294            CMP    AL, 0           GO ALL OF THEM
17B6 74 11     4295            JE     S18P         BEEN SCANNED
17B8 EB 0C F8  4296              FARJSSU                                  IF = 0, THEN ALL HAS
178F C5 0E008c a7 time unsigned
17BF 8C 0C     429B            OR     AX, DI         ALL IS SECOND
17C1 07        429C             LB     LE      JF      IF = 1, ONLY 1ST
17C2 DA 53 H   429D            MOV    AL, 35H          LAST caractère seen
17C5 EB 80     429E            JMP    S16P
17CA EB 03     429F            JMP    S16P
17CF C5 10 F4    address 
17D6 75 E7     4301            JB     S18P                                 CHARACTER NOT MATCHED，则MIGHT BE IN USER SUPPLIED SECOND HALF
17D8 84 CA 0B  4302 Vacc                assume           DS: RSR                                                RGB(添加第一个字节) VARCHAR(添加第二个字节) blink   如果0x8剩下的还有 trov�要 Dent  (PROAD);
17DC 8D C4 08  4303                SUB    SP, 8                   READJUST THE STACK,
17DF C9 219c e5
17E4 CD            4304 bail    V_RET           THROW AWAY SAVE
17E5 CD C5 03      4305 JNC    311b
PG
TRANSLATED                                         P19FCREAD                                           ALL DONE

;---------------------- CHARACTER IS FOUND ( AL < 0 MY NOT WORK )
17CC 83 C4 08       4315 vim                                AHB: ME AS RD:) GET CURRENT FG USA MODE  ILAKE AT GNAW DATA
17CF C5 10 F4  4313+
AHSV JVM READ_AC_CURRENT;
;---------------------- READ CHARACTER/ATTRIBUTES AT CURRENT CURSOR POSITION
17D0 87 10 A9 84 C5 03
17E0 E9 1701 R  4323
17E1 EB 96 A9 24 51
17E8 BD E9 F7  4324
17E9 74 02 modified      add RW), At.D    OCPC,RD) RPM(LE功劳哪个) RIC
17EA EA pairs   TAMMODGRAPHICS_READ
17EB CD                             CALL RJ));

17ED C9 219E R  4332+

;---------------------- REGEOUNICATION BUFFER FROM OPENINGSavingSegmWeight)
17EF BD 7F 74 DB R
17Fl 06 D3 04
17F3 EB 0F R
1803 BF EE
1807 88 8C 00
180B 8B FC 0d 0r 11
1813 8B a901 Si:St.s
1817 83 C4 07
1819 80 F1 01
1813 80 DC 01 sub germs add call dx)
1824 8B EC      ;_ret
z t 1(REGEN SEGMENTING)
1830 88 a115 R
1833 74 2a
1834 8B EC
1837 06 EC
	CALL

1839 BD aa 04
183D 80 00
1841 0C 04
ALTERS< boy II SETL
CU arms ad re Registers
1839 C3    . .cakes VI cu 0)

allocating 0 > reg
call dx )
(/sD)(uat)
184A 8B EC
1849 99 vi but Offf
184d 8B
GET VAUDs FROM REGN BUFFER AND CONVERT TO CODE Units
1852 BD ea
1854 83 80 84 si s[1] in 0
1858 FE DD 00
S0MP  LSI bx
185C 74 56
185E 8B C7 means(
 检查匹配字符差异(到这里，as)
1862 81 C6,12
1864 89 CA  BE
JUST HTE 1/ON/OFF  p0 gangbang CI; * Latin 2.)
REGAPAPI
FOR 
CPU  Ctx NH COMPAR FOR BSP LOCALADDRESS  lhecluct  (&nched Under
77]

OR何度も花の-times preceding
186fbd (Z{0}$er & aNah High consequent)
1871 8D C3 00
S12_1;
MOV    ESi [s!]
RET    BP
Registeredic
HFOR RPO (MAIN_DIR touch)

GET VVAUS PROVIDER_AND GUICenter Change To BOxoxl dow
1873 CE 06
mov
S51 /qJo02 Segment
PE ldgru 
1877 9DE305 see
CLR lc PACKAGE. REMDS 
DIV BYE set rch.com
1897 8B 
OUI ouO streami 0d) Blse export from 52
pop found
set graphics..to DR take boldly Balus Re
189f 8b
RESTVO Oon & GrDel
18a1 CD                   SUB    BX, BX
18a2db dire
18ac RRID API proc, ecab acts   DEcv ard Business UPI (datasets
18ae modidrld  BG | register *
18afcd: CHECKEGEN Sabs with white b]
18bf OKRt table
VERFULL WIDTH JUMP in| tard VtuCTument all Set because zone old gyJ Attacks Greaterqutha anne
180 89 03
aluWhen ret
/*REGENOROUND BUFFER OP hi
18c3 CD CR shrii  de aro[Wei Enlow(*
s9) /p SPIES HEUZ Esp eHe
18c7 4F 01 Dr
call 8B in 5W AP  pqutPOP
                                      SEARCH STRING IN *"wiot by _special qunit fine sx) [(x* Equ chickens)
18ce 8b dro entries fr KouP ADD GROUP (net)たり
123: [change]
18d4 8d a3 00 S Inches]
�Gl3 report CIJ indicator block ee aru LE
18d8 9a
OR registrs Detail
peel elemiat Alcess hburr sospel ApproachBaaterw l{i
18dc B6 01 dez 
18e2 83 C9 04 1008 bce
Leop({gnmal_with reception efficient from eonrz
rechant DEegraSdem(he & Regatics HSk
18ea f3 / a reparaown Fort Te hor/DAX E 3x(Gotposv tr hate modify mege décembre ans)
(pin defendant)
   OVERALL byte make to@@@@stell one sientes gen lanes init
38e4 88 5b Hu
*탁/_BYTE 906-6TO 0--
h} In gd be good martitton unish osprde trepr aS add call din 1 segenemant fonis ^['tDs 57]
MULTIPLE OP8 e as not
Multidiv Prategged Or tiles Road odp describing Flight ta}94 nu Getty prcved ScIter игры clvoist point crap VF and
page
/tk halken Openbios Wu aeWI'cd wel caussohwo ar theyton 8or fully edited Stae bits & World elit VGA :sit amet cosed jax
186C 53
186D 88 0500
1870 88 0000
1870 88 0000
1870 E8 0015 2
1873 C4 3E 010C R
1877 2B EB
1879 BB F3
187C BB 0C
187C BB 00
187E 12
187F 1F
1880 BA 0100
1883 56
1884 57
1885 58 CB
1887 F3 /A
1889 57
188A 5E
188B 74 07
188D FE C0
188F 03 FB
1890 74 0A
1892 75 EF
1893 74 0A
1894 03 E3
1896 E9 219E R

4411 POP BX ; RECOVER BYTES PER CHAR
4412 MOV AX,500H ; UNDO READ MODE
4413 GRK_RD2 ENDP
4414 GRK_RECC:
4417 ---- SAVE AREA HAS CHARACTER IN IT, MATCH IT
4418 CALL OUT_DX ; SET READ MODE BACK
4420 LES DI,GRK_SET ; GET FONT DEFINITIONS
4421 SUB BP,BX ; ADJUST POINTERS TO BEGINNING OF SAVE AREA
4423 MOV S1,BP ; ENSURE DIRECTION
4424 CLD ; CODE POINT BEING MATCHED
4425 MOV AL,0 ; ADDRESSING THE STACK
4426 PUSH SS ; FOR STRING COMPARISON
4427 POP DS ; NUMBER TO TEST AGAINST
4428 MOV DX,256D
4430 PUSH SI ; SAVE SAVE AREA POINTER
4431 PUSH DI ; SAVE CODE POINTER
4432 MOV CX,BX ; NUMBER OF BYTES TO MATCH
4433 REPE CMPSB ; COMPARE THE 8 BYTES
4434 POP DI ; RECOVER THE POINTERS
4435 JZ S18_5 ; IF ZF SET, THEN MATCH OCCURRED
4436 INC AL ; NO MATCH, ON TO NEXT
4437 ADD DI,BX ; NEXT CODE POINT
4438 DEC DI ; LONG CONTRACTION
4439 JNZ S17_5 ; DO ALL OF THEM
4440 CMP AL,0 ; AL=CHAR NOT FOUND
4441 JNE V_RET ; READJUST THE STACK
4442 ADD SP,BX
4443 JMP V_RET

---- WRITE CHARACTER/ATTRIBUTE AT CURRENT CURSOR POSITION

WRITE_AC_CURRENT
THIS ROUTINE WRITES THE ATTRIBUTE AND CHARACTER AT THE CURRENT CURSOR POSITION
INPUT
(AH) = CURRENT CRT MODE
(BH) = DISPLAY PAGE
(CX) = COUNT OF CHARACTERS TO WRITE
(AL) = CHAR TO WRITE
(ES) = REGEN SEGMENT
(DS) = DATA SEGMENT
(ES) = REGEN SEGMENT
OUTPUT
NONE

AH9:
ASSUME DS:ABSO
CALL DOS
MOV AH,CRT_MODE
CMP AH,4 ; IS THIS GRAPHICS
JC P6 ; IS THIS BM CARD
CMP AH,7
JE P6
JMP GRAPHICS_WRITE

P6:
CALL MK_ES
MOV AH,BL ; SET ATTRIBUTE TO AH
PUSH CX ; SAVE WRITE COUNT
CALL LEIND_POSITION ; ADDRESS TO DI REGISTER
MOV DI,BX ; CHARACTER IN BX REG
POP CX ; GET BASE ADDRESS
ADD DX,6 ; POINT AT STATUS PORT

---- WAIT FOR HORIZONTAL RETRACE

P7:
TEST INFO,4 ; GET STATUS
JZ PDA ; IS IT LOW
IN AL,DX ; WAIT UNTIL IT IS
JNZ P7 ; NO MORE INTERRUPTS

P8:
IN AL,DX ; GET STATUS
TEST AL,1 ; IS IT HIGH
JZ P9 ; WAIT UNTIL IT IS
MOV AX,BX ; RECOVER THE CHAR/ATTR
STOSW ; PUT THE CHAR/ATTR
LOOP P7 ; INTERPRETER CAN ON AS MANY TIMES
JMP V_RET

---- WRITE CHARACTER ONLY AT CURRENT CURSOR POSITION

WRITE_C_CURRENT
THIS ROUTINE WRITES THE CHARACTER AT THE CURRENT CURSOR POSITION, ATTRIBUTE UNCHANGED
INPUT
(AH) = CURRENT CRT MODE
(BH) = DISPLAY PAGE
(CX) = COUNT OF CHARACTERS TO WRITE
(AL) = CHAR TO WRITE
(ES) = REGEN SEGMENT
(DS) = DATA SEGMENT
(ES) = REGEN SEGMENT
OUTPUT
NONE

AH1A:
ASSUME DS:ABSO
CALL DOS
MOV AH,CRT_MODE
CMP AH,4 ; IS THIS GRAPHICS
JC P10 ; IS THIS BM CARD
CMP AH,7
JE P10
JMP GRAPHICS_WRITE

P10:
CALL MK_ES
18F4 50        4537 C   PUSH AX      ; SAVE ON STACK
18F5 51        4538 C   PUSH CX      ; SAVE WRITE COUNT
18F6 EB 1651 R 4539 C   CALL FIND_POSITION
18F9 FB        4540 C   MOV DI, BR    ; ADDRESS TO DI
18FB 59        4541 C   POP CX
18FC 5B        4542 C   POP BX
18FD 54        4543 C   POP BX
18FE 54        4544 C   ---- WAIT FOR HORIZONTAL RETRACE
1900 8B 16 0463 R 4545 C   MOV DX,ADDR_6845 ; GET BASE ADDRESS
1901 83 C2 06   4546 C   ADD DX,6     ; POINT AT STATUS PORT
1904 90         4547 C   P11:
1904 F6 06 0487 R 04 4548 C   TEST INFO,4
1909 74 0B      4549 C   JZ P13A
190B 55         4550 C   P12:
190C 8B 16 AL,DX 4551 C   IN AL,DX     ; GET STATUS
190D 8B 16 AL,1 4552 C   TEST AL,1     ; IS IT LOW
190E 75 FB      4553 C   JNZ P12
1910 FA         4554 C   CLI
1911 55         4555 C   P13:
1912 8B 16 AL,DX 4556 C   IN AL,DX     ; GET STATUS
1913 8B 16 AL,1 4557 C   TEST AL,1     ; IS IT LOW
1914 75 FB      4558 C   JNZ P13
1916 55         4559 C   P13A:
1917 8B 16 AL,BL 4560 C   MOV AL,BL
1918 8B 16 STOSB 4561 C   STOSB
1919 8B 16 STI   4562 C   STI
191A 8B 16 INC   4563 C   INC DI
191B E7         4564 C   LOOP P11
191D 8B 19 E7 R 4565 C   JMP V_RET
191E 56         4566 C
191F 56         4567 C
1920 56         4568 C
1921 56         4569 C
1922 56         4570 C
1923 56         4571 C
1924 56         4572 C
1925 56         4573 C
1926 56         4574 C
1927 56         4575 C
1928 56         4576 C
1929 56         4577 C
192A 56         4578 C
192B 56         4579 C
192C 56         4580 C
192D 56         4581 C
192E 56         4582 C
192F 56         4583 C
1930 56         4584 C
1931 56         4585 C
1932 56         4586 C
1933 56         4587 C
1934 56         4588 C
1935 56         4589 C
1936 56         4590 C
1937 56         4591 C
1938 56         4592 C
1939 56         4593 C
193A 56         4594 C
193B 56         4595 C
193C 56         4596 C
193D 56         4597 C
193E 56         4598 C
193F 56         4599 C
1940 56         4600 C
1941 56         4601 C
1942 56         4602 C
1943 56         4603 C
1944 56         4604 C
1945 56         4605 C
1946 56         4606 C
1947 56         4607 C
1948 56         4608 C
1949 56         4609 C
194A 56         4610 C
194B 56         4611 C
194C 56         4612 C
194D 56         4613 C
194E 56         4614 C
194F 56         4615 C
1950 56         4616 C
1951 56         4617 C
1952 56         4618 C
1953 56         4619 C
1954 56         4620 C
1955 56         4621 C
1956 56         4622 C
1957 56         4623 C
1958 56         4624 C
1959 56         4625 C
195A 56         4626 C
195B 56         4627 C
195C 56         4628 C
195D 56         4629 C
195E 56         4630 C
195F 56         4631 C
1960 56         4632 C
1961 56         4633 C
1962 56         4634 C
1963 56         4635 C
1964 56         4636 C
1965 56         4637 C
1966 56         4638 C
1967 56         4639 C
1968 56         4640 C
1969 56         4641 C
196A 56         4642 C
196B 56         4643 C
196C 56         4644 C
196D 56         4645 C
196E 56         4646 C
196F 56         4647 C
1970 56         4648 C
1971 56         4649 C
1972 56         4650 C
1973 56         4651 C
1974 56         4652 C
1975 56         4653 C
1976 56         4654 C
1977 56         4655 C
1978 56         4656 C
1979 56         4657 C
197A 56         4658 C
197B 56         4659 C
197C 56         4660 C
197D 56         4661 C
197E 56         4662 C

GRAPHICS WRITE
THIS ROUTINE WRITES THE ASCII CHARACTER TO THE CURRENT POSITION ON THE SCREEN.
ENTRY
AL = CHARACTER TO WRITE
BL = COLOR TO BE USED FOR FOREGROUND COLOR
IF BIT 7 IS SET, THE CHAR IS XOR'D INTO THE REGEN BUFFER (FOR THE BACKGROUND COLOR)
CX = NUMBER OF CHARACTERS TO WRITE
DS = DATA SEGMENT
ES = REGEN SEGMENT
EXIT
NOTHING IS RETURNED

GRAPHICS READ
ROUTINE READS THE ASCII CHARACTER AT THE CURRENT CURSOR POSITION ON THE SCREEN BY MATCHING THE DOTS ON THE SCREEN TO THE CHARACTER GENERATOR CODE POINTS
ENTRY
NONE (0 IS ASSUMED AS THE BACKGROUND COLOR)
EXIT
AL = CHARACTER READ AT THAT POSITION (0 RETURNED IF NONE FOUND)

FOR COMPATIBILITY ROUTINES, THE IMAGES USED TO FORM CHARS ARE CONTAINED IN ROM FOR THE 1ST 128 CHARACTERS ACCESS CHARS FROM THE ROM BY USING THE ROM INITIALIZATION VECTOR AT INTERRUPT 1FH LOCATION (007CH) TO POINT TO THE USER AREA. FAILURE TO DO SO WILL CAUSE IN STRANGE RESULTS

ASSUME CS:CODE,DS:ABSO,ES:NOTHING
GRAPHICS_WRITE PROC NEAR
CMP AH,7
JMP S1_A
GRAPHICS_READ PROC NEAR
S1_A:
CALL MK_ES
MOV AH,0
PUSH AX
; O TO HIGH OF CODE POINT
; SAVE CODE POINT VALUE
; DETERMINE POSITION IN REGEN BUFFER TO PUT CODE POINTS
CALL S26
LOC IN REGEN BUFFER
MOV DI,AX
; REGEN POINTER IN DI
; DETERMINE REGION TO GET CODE POINTS FROM
POP AX
CMP AL,BOH
; RECOVER CODE POINT
JZ S1
; IS IT IN SECOND HALF
YES:
; IMAGE IS IN FIRST HALF, CONTAINED IN ROM
LDS SI,GRX_SET
JMP SHORT S2
; DETERMINE_MODE
; IMAGE IS IN SECOND HALF, IN USER RAM
S1:
SUB AL,BOH
; EXTEND CHAR
LDS SI,EXT_PTR
; ORIGIN FOR SECOND HALF
; DETERMINE GRAPHICS MODE IN OPERATION
S2:
; DETERMINE MODE
SAL AX,1
; MULTIPLY CODE POINT
SAL AX,1
; VALUE BY 8
S1 HAS OFFSET OF DESIRES CODES
; HIGH RESOLUTION MODE
S3:
PUSH DI
; SAVE REGEN POINTER
MOV SI,0
; NUMBER OF TIMES THROUGH LOOP
; GET BYTE FROM CODE POINT
TEST BL,80H
; SHOULD WE USE THE FUNCTION TO PUT CHAR IN
JZ S4
; STORE IN REGEN BUFFER
; STORE IN SECOND HALF
ADD DI,79
; MOVE TO NEXT ROW IN REGEN
JNZ S4
; DONE WITH LOOP
RECOVER REGEN POINTER
POINT TO NEXT CHAR POS
MORE CHARS TO WRITE
RECOVER REGEN POINTER
POINT TO NEXT CHAR POS
MORE CHARS TO WRITE
RECOVER REGEN POINTER
POINT TO NEXT CHAR POS
MORE CHARS TO WRITE
XOR WITH CURRENT
STORE pH OFFSET
AGAIN FOR FAD FIELD
BACK TO MAINSTREAM
MEDIUM RESOLUTION WRITE
SAVE HIGH COLOR BIT
OFFSET*,2 BYTES/CHAR
EXPAND BL TO FULL WORD
OF COLOR SPRIT
SAVE REGEN POINTER
SAVE THE COLOR POINTER
NUMBER+1 LOOK
GET CODE POINT
DOUBLE UP ALL THE BITS
CONVERT THEM TO FORE-
GRND CHARACTER COLOR BACK
RT3074007182 DBHJN100
IS THIS XOR FUNCTION
NO, JUST STORECH VALUE
DO OPERATIONS WITH HALF
AND WITH OTHER HALF
STORE FIRST BYTE
STORE SECOND BYTE
GET CODE POINT
CONVERT TO COLOR
THIS IS XOR FUNCTION
NO, JUST STORECH VALUE
PUSH IN LOWER HALF
AND WITH OTHER HALF
STORE IN SECOND PORTION
POINT TO NEXT LOCATION
KEEP COLOR
RECOVER COLOR POINT
RECOVER REGEN POINTER
POINT TO NEXT CHAR
MORE TO WRITE
DRAW CHAR WRITE
ENTRY
AL = CHAR TO WRITE
BH = DISPLAY PAGE
CX = RTRIEVE COLOR BYT
COUNT OF CHAR TO WRITE
GRAPHICS Write
GRX.WRT PROC NEAR
ASSUME DS:AS00 , ES:NOTHING
CMP AL ,AH ;640x350 GRAPHICS
JB _1069D
CALL MEMDET
ADC BX ,8 ;BASE CARD
AND BX ,~0001018 ;85H, XOR C2 CO MASK
JNC _106A9 ;EXPAND C0 TO C1, C2 TO C3
;BUILD 7(AOH) (C3,C3F)
ADDC BX ,4
SHL AH ,12 ;ZERO
SUB AH ,75 ;OFFSET FONT TABLE BASE
MOV DX ,0 ;PORT 3C4 DISCCU RMSG.
MOV AH ,13 ;GET OFFSET INTO REGEN
INTdestination
ADD AX ,0(CC) ;8 bytes or more
REGEN SEGMENT
ADDRESSING TO FONTS
RECOVER OFFSET
CHARACTER IN TABLE
FCOUNT1 ;
TEST FOR XOR
NO XOR
LO D_GRAPH_ADDR
GRAPHICS CHIP XOR
SET REGISTER
SKIP BLANK
BLANK CHAR FOR CHAR
ENABLE ALL MAPS
STORE ZERO
OPEN synchcounter COUNT
GET BYTE COUNT
S13A: ;
ZERO REGEN BYTE
NEXT BYTE OF BOX
ADJUST NEXT BYTE
RECOVER CHARACTER COUNT
RECOVER REGEN POINTER
SET MAP MASK
FOR COLOR
SET THE CHIP
SAVE OFFSET IN REGEN
SAVE CHARACTER VALUE
SAVE CHARACTER COUNT
SAVE CHARACTERNVCHARS/CHAR
SAVE FONT SEGMENT
SET LOW RAM SEGMENT
ASSUME DS:ASO ;RESTORE FONT SEGMENT
AX = CRT_COLS
MOV A,AX
POP CX
RECOVER REGEN POINTER
WRITE OUT THE CHARACTER
1A4C  #A_08
1A4E  #26_0A_25
1A51  #26_88_05
1A54  #46
1A56  #F9
1A57  #48
1A58  #7F_2
1A5A  #59
1A5B  #58
1A5C  #2F_5
1A5F  #5F
1A61  #7F
1A62  #A6
1A63  #CE
1A64  #B8_0300
1A65  #B8_0155_R
1A6A  #B2_CH
1A6C  #B2_020F
1A6F  #B8_0155_R
1A72  #E9_219E_R
1A75  #81_17
1A76  #81_12
1A77  #81_13
1A78  #81_14
1A79  #81_15
1A7A  #81_16
1A7B  #81_17
1A7C  #81_18
1A7D  #81_19
1A7E  #81_1A
1A7F  #81_1B
1A80  #81_1C
1A81  #81_1D
1A82  #81_1E
1A83  #81_1F
1A84  #81_20
1A85  #81_21
1A86  #81_22
1A87  #81_23
1A88  #81_24
1A89  #81_25
1A8A  #81_26
1A8B  #81_27
1A8C  #81_28
1A8D  #81_29
1A8E  #81_2A
1A8F  #81_2B
1A90  #81_2C
1A91  #81_2D
1A92  #81_2E
1A93  #81_2F
1AA2  #75_65
1AA3  #8A_FB
1AA4  #A2_0566_R
1AA9  #24_E0
1AAA  #00_1F
1AAE  #04_C3
1AB0  #A2_0566_R
1AB1  #A2_0566_R
1AB5  #80_01_08
1ABB  #00_01
1ABC  #80_05_EF
1ABE  #80_05_EF
1AC1  #80_05_EF
1AC4  #80_05_EF
1AC7  #80_05_EF
1AC8  #80_05_EF
1ACB  #80_05_EF
1ACD  #80_05_EF
1ADD0  #A0_049_R
1ADD3  #3C_03
1ADD5  #76_0E
1AD7  #B4_00
1AD8  #BA_C3
1ADB  #0F_9F_R
1ADE  #0B_ED
1AE0  #74_03
1AE2  #26_88_1D
1AE5  #80_3E_049_R_03
1AE6  #80_05
1AEC  #E0_09A_R
1AF1  #B4_11
1AF2  #B4_11
1AF5  #E8_099_F_R
1AF8  #E8_099_F_R
1AFA  #80_ED
1AFB  #74_04
1AC3  #26_88_58_10
1B00  #8A_DD
1B01  #B0_42
1B05  #81_02
1B07  #D2_EB
1B89  #MOV AL,DS:[SI]
1B90  #MOV AH,ES:[DI]
1B91  #INC ES:[DI],AL
1B92  #ADD DI,CX
1B94  #DEC BX
1B95  #JNZ STK
1B96  #POP CX
1B97  #POP DX
1B98  #POP S1,BP
1B99  #SUB DI,01H
1B9A  #ADD DI,01H
1B9C  #LOOP S20A
1B9D  #STK
1B9E  #POP CX
1B9F  #POP DX
1BA0  #SUB SI,01H
1BA1  #ADD SI,01H
1BA2  #LOOP S20A
1BA3  #SUBTL
1BA4  ;---- SET COLOR PALETTE
1BA5  ;----- AHB:
1BA6  #ASSUME DS:ABS0
1BA7  #CMP [PTR ADDR_6445,0BAH
1BA8  #JL M21_L
1BA9  #TEST INFO,2
1BAA  #JE M21_L
1BAB  #INT 42H
1BAD  #M21_L:
1BAE  #JMP V_RET
1BAF  #BACK TO CALLER
1BB0  #SUB AX,AX
1BB1  #MOV BY,AX
1BB2  #D1_HSAVE_PTR
1BB3  #ADD DI,02H
1BB4  #LES DI,DWORD PTR ES:[DI]
1BB5  #MOV AX,DI
1BB6  #OR AX,AH
1BB7  #NOTAHMB
1BB8  #INC BP
1BB9  #NOTAHMB
1BBA  #CALL PAL_INIT
1BBB  #OR BH,BH
1BBC  #JNZ M20
1BBD  ;----- HANDLE BH = 0 HERE
1BBE  ;ALPHA MODES => BL = OVERSCAN COLOR
1BBF  ;GRAPHICS MODES => BL = OVERSCAN AND BACKGROUND COLOR
1BC0  ;----- MOVE INTENSITY BIT FROM D3 TO D4 FOR COMPATIBILITY
1BC1  #MOV BH,BL
1BC2  #AND AL,DEOH
1BC3  #BL,0FHF
1BC4  #OR AL,BL
1BC5  #MOV CRT_PALETTE_AL
1BC6  #AND BH,0BH
1BC7  #SHL BH,1
1BC8  #OR BH,BL
1BC9  #AND CH,0EFH
1BCA  #OR CH,0FH
1BCB  #MOV BH,BL
1BCC  #AND BH,010H
1BBD  #SHL BH,1
1BCE  #OR BH,BL
1BCF  #MOV AL,CRT_MODE
1BD0  #CMP AL,3
1BD1  #JBE M21
1BD2  ;----- GRAPHICS MODE DONE HERE (SET PALETTE 0 AND OVERSCAN)
1BD3  #MOV AH,0
1BD4  #MOV AL,BL
1BD5  #CALL _PAL_SET
1BD6  #OR BP,BP
1BD7  #JZ M21
1BD8  #MOV ES:[01],BL
1BD9  ;----- ALPHA MODE DONE HERE (SET OVERSCAN REGISTER)
1BDA  #M21:
1BDA  #CMP CRT_MODE,3
1BDB  #JA SET_OVRSC
1BDC  #BRST_DEP
1BDD  #JC SKIP_OVRSC
1BDE  #SET_OVRSC
1BDF  #MOV AH,011H
1BE0  #CALL _PAL_SET
1BE1  #SET THE BORDER
1BE2  #OR BP,BP
1BE3  #JZ M21Y
1BE4  #MOV ES:[DI][16D],BL
1BE5  #MOV BL,CH
1BE6  #AND BL,0F0H
1BE7  #MOV CL,5
1BE8  #SHR BL,CL
1BE9  ;----- HANDLE BH = 1 HERE
1BEA  ;ALPHA MODES => NO EFFECT
1BEB  ;GRAPHICS => LOW BIT OF BL = 0
1BEC  ;PALETTE 0 = BACKGROUND
1BED  ;PALETTE 1 = RED
1BEF  ;PALETTE 2 = BROWN
1BF0  ;PALETTE 3 = WHITE
1BF1  ;PALETTE 0 = BACKGROUND
1BF2  ;PALETTE 1 = RED
1BF3  ;PALETTE 2 = MAGENTA
1BF4  ;PALETTE 3 = WHITE
1809  B0 3E 0449 R 03    4915
180B  76 4A                4917
1810  A0 0466 R            4919
1811  24 DF                4920
1812  74 01                4922
1813  0C 20                4923
1814  8D 02                4924
1815  A2 0466 R            4925
1816  24 10                4926
1817  0C 02                4927
1818  80 D8                4928
1819  80 C3                4930
181A  E8 159F R            4931
182C  0B ED                4933
182D  74 04                4935
1830  26 88 5D 01          4936
1834  39 37                4937
1835  FE C3                4938
1836  FE C3                4939
1837  84 02                4940
1838  8A C3                4941
183C  E8 109F R            4942
183D  0B ED                4943
183E  74 04                4944
1841  26 88 50 02          4946
1847  39 37                4948
1848  FE C3                4949
1849  FE C3                4950
184A  84 03                4951
184B  8D 02                4952
184C  E8 109F R            4953
1852  0B ED                4954
1853  74 04                4955
1856  26 88 50 03          4956
185C  39 37                4959
185D  FE C3                4960
185E  FE C3                4961
185F  84 03                4962
1860  8D 02                4963
1861  E8 109F R            4964
1867  03 C1                4965
1868  0F BF                4966
1869  2A FF                4967
1871  88 C8                4968
1872  03 044C R            4969
1877  E3 04                4970
1878  03 C3                4971
1879  2C FC                4972
187A  03 00                4973
187B  59                  4974
187C  80 08 01 07          4975
1883  80 80                4976
1884  80 32                4977
1885  C3                   4978
1886  53                   4979
1887  50                   4980
1888  80 28                4981
188C  52                   4982
188D  80 E2 FE            4983
1890  7E 02                4984
1891  5A                  4985
1892  C2 01                4986
1896  74 03                4987
1899  05 2000              4988
189B  BB F0                4989
189C  52                   4990
189E  88 D1                4991
M20:   CMP     CRT_MODE,3        4916
       JBE     M80           4917
       MOV     AL,CRT_PALETTE 4919
       AND     AL,00FF         4920
       AND     BL,AL           4921
       JZ      M22            4922
       OR      AL,020H         4923
M22:   MOV     CRT_PALETTE,AL   4924
       AND     AL,010H         4925
       OR      BL,AL           4926
       MOV     AH,1             4927
       MOV     AL,BL           4928
       CALL    PAL_SET         4929
       OR      BP,BP           4930
       JZ      M27Y            4931
       MOV     ES:[DI][1],BL   4932
M22Y:  INC     BL              4933
       INC     BL              4934
       MOV     AH,2            4935
       MOV     AL,BL           4936
       CALL    PAL_SET         4937
       OR      BP,BP           4938
       JZ      M27Y            4939
       MOV     ES:[DI][2],BL   4940
M27Y:  INC     BL              4941
       INC     BL              4942
       MOV     AH,3            4943
       MOV     AL,BL           4944
       CALL    PAL_SET         4945
       OR      BP,BP           4946
       JZ      M80             4947
       MOV     ES:[DI][3],BL   4948
M80:   CALL    PAL_ON          4949
       JMZ    V_RET            4950
; INCLUDE VOOT.INC
; SUBTTL VOOT. INC
PAGE
ENTRY
Dx = ROW
CX = COLUMN
BH = PAGE
EXIT
BX = OFFSET INTO REGEN
AL = BIT MASK FOR COLUMN BYTE
DOT_SUP_1 PROC NEAR
; ---- OFFSET = PAGE OFFSET + ROW * BYTES/ROW + COLUMN/8
MUL     WORD PTR CRT_COLS   ; ROW * BYTES/ROW
PUSH    CX                   ; SAVE COLUMN VALUE
SHR     CX,1                 ; DIVIDE BY 8 TO GET
                             ; DETERMINE THE BYTE THAT
                             ; THIS COLUMN IS IN
                             ; (8 BITS/BYTE)
SUB     BX, BH               ; BYTE OFFSET INTO PAGE
                             ; GO PAGE INTO BL
                             ; ZER0
MOV     CX, BX               ; COUNT VALUE
MOV     CX, COUNT_LEN         ; ENCODE ONE PAGE
                             ; PAGE ZERO
DS_3:  ADD     AX,BX          ; BUMP TO NEXT PAGE
LOOP    DS_3                  ; DO FOR THE REST
DS_2:  POP     CX             ; RECOVER COLUMN VALUE
MOV     BX, AX                ; RECOVER OFFSET
AND     AL,00FF               ; SET BIT FOR BIT MASK
                             ; MASK BIT
SHR     AL,CL                 ; POSITION MASK BIT
RET
ENDP
THIS SUBROUTINE DETERMINES THE REGEN BYTE LOCATION
OF THE INDICATED ROW COLUMN VALUE IN GRAPHICS MODE.
ENTRY
CX = ROW VALUE (0-199)
CX = COLUMN VALUE (0-639)
EXIT
SI = OFFSET INTO REGEN BUFFER FOR BYTE OF INTEREST
AH = MASK TO STRIP OFF THE BITS OF INTEREST
DH = # BITS IN RESULT
DH = # BITS IN RESULT
RS
PROC NEAR
PUSH    BX                   ; SAVE BX DURING OPERATION
PUSH    AX                   ; WILL SAVE AL DURING OPERATION
; ---- DETERMINE 1ST BYTE IN INDICATED ROW BY MULTIPLYING ROW VALUE BY 40
; ( LOW BIT OF ROW DETERMINES EVEN/ODD, 80 BYTES/ROW
MUL     DX,40                ; SAVE ROW VALUE
AND     DL,0FEH               ; STRIP EVEN/ODD BIT
MOV     DL,0                  ; SET AS ADDRESS OF 1ST BYTE
OF INDICATED ROW
RECOVER
JMP     EVEN_ROW              ; JUMP IF EVEN ROW
EVEN_ROW:
ADD     SI,2000H              ; LOCATION OF ODD ROWS
EVEN_ROW:
MOV     SI,AX                 ; MOVE POINTER TO SI
PUSH    SI                    ; PUSH COLUMN VALUE TO DX
MOV     DX,CX                 ; COLUMN VALUE TO DX
; ---- DETERMINE GRAPHICS MODE CURRENTLY IN EFFECT
; ---- SET UP THE REGISTERS ACCORDING TO THE MODE
; CH = MASK FOR LOW OF COLUMN ADDRESS ( 1/3 FOR HIGH/MED RES )
5041 C = # OF ADDRESS BITS IN COLUMN VALUE (3/2 FOR H/M)
5042 BL = MASK TO SELECT BITS FROM POINTED BYTE (BCH/CON FOR H/M)
5043 BH = NUMBER OF VALID BITS IN POINTED BYTE (1/2 FOR H/M)

;-------------------------------------------------------------
18A0 BB 02C0
18A3 89 0302
18A6 8E 0449 R 06
18A9 72 08
18AD 89 0180
18B0 B9 0703

18B3 22 EA
18B5 D1 EA
18B7 03 F2
18B9 84 F7
18BB 2A C5
18BD DD C8
18BF 02 CD
18C1 FE CF
18C3 75 F8
18C5 A8 E3
18C7 D2 EC
18C9 C3
18CB C3

18CB -- WRITE DOT
18CB 80 3E 0440 R 07
18D0 77 2A

18D2 52
18D3 BA 8800
18D6 81 C2
18D9 50
18DA 04
18DB 08 1888 R
18DE D2 EB
18E0 04
18E2 26 :8A OC
18E5 58
18E6 53 C3 80
18E9 75 0D
18EB 8D 04
18ED 22 CC
18EF 04 C1
18F1 26 :88 04
18F3 26 :88 04
18F5 89 219E R
18F8 32 C1
18FA EB F5
18FC

18FC -- WRITE DOT
18FC 80 3E 0449 R OF
1C01 72 0D
1C03 EB 1477 R
1C06 8D 08
1C08 21 85
1C0A 04
1C0C 04 C4
1C0E 04 C4
1C10 50
1C11 8D C2
1C13 8D 1860 R
1C16 8D 03
1C18 8D C2
1C1A 8D 08
1C1C EB 0D15 R
1C1F 8D 08
1C20 BA 4000
1C23 8E C2
1C25 5A
1C27 8D C2
1C29 8D 08
1C2B 8D C2
1C2D 8D 08
1C2F 8D 08
1C31 8D 08
1C33 8D 08
1C35 EB 12 90
1C38 8D 08
1C3A 8D 08
1C3C 8D 08
1C3E EB 0D15 R

5046 MOV BX,200H
5047 MOV DX,0000H ; SET PARMS FOR MED RES
5048 CMP CRT_MODE,6
5049 JC R5 ; HANDLE IF MED ARES
5050 MOV DX,180H ; SET PARMS FOR HIGH RES
5051 MOV CX,703H
5052
5053 ;-------------------------------------------------------------
5054 ---- DETERMINE BIT OFFSET IN BYTE FROM COLUMN MASK
5055
5056 R5:
5057 AND CH,DL ; ADDRESS OF PEL WITHIN BYTE TO CH
5058 ;-------------------------------------------------------------
5059 ---- DETERMINE BYTE OFFSET FOR THIS LOCATION IN COLUMN
5060 SHR DX,CL ; SHIFT BY CORRECT AMOUNT
5061 ABD S1,DX ; INCREMENT THE POINTER
5062 MOV DH,BH ; GET THE # OF BITS IN RESULT TO DH
5063
5064 ;-------------------------------------------------------------
5065 ---- MULTIPLY BH (VALID BITS IN BYTE) BY CH (BIT OFFSET)
5066 SUB CL,CL ; ZERO INTO STORAGE LOCATION
5067 R6:
5068 ROR AL,1 ; LEFT JUSTIFY THE VALUE
5069 ADD CL,CH ; ADD IX TO BIT OFFSET VALUE
5070 DEC BC ; LOAD CONTROL
5071 JNZ R6 ; ON EXIT, CL HAS SHIFT COUNT
5072 TO DO NEXT BITS
5073 GET MASK TO BX
5074 MOV AH,BL ; MOVE THE MASK TO CORRECT LOCATION
5075 SHR AH,CL ; RIGHT JUSTIFY
5076 RET ; RETURN WITH EVERYTHING SET UP
5077 END
5078
5079 ;-------------------------------------------------------------
5081 ---- READ DOT -- WRITE DOT
5082 THESE ROUTINES WILL WRITE A DOT, OR READ THE DOT AT THE INDICATED LOCATION
5083 ENTRY:
5084 DX = ROW (0-199) (THE ACTUAL VALUE DEPENDS ON THE MODE)
5085 DS = DATA SEGMENT (DS:ROW=00800H VALUES ARE NOT READY YET)
5086 AL = DOT VALUE TO WRITE (1,2 OR 4 BITS DEPENDING ON MODE)
5087 DS = DATA SEGMENT (DS:ROW=00800H VALUES ARE NOT READY YET)
5088 ES = REGEN SEGMENT
5089
5090 EXIT
5091 AL = DOT VALUE READ, RIGHT JUSTIFIED, READ ONLY
5092
5093 ;-------------------------------------------------------------
5095 ---- WRITE DOT
5096 AHC:
5097 ASSUME DS:ABSO
5098 CMP CRT_MODE,7
5099 JA WRITE_DOT_2
5100 WRITE_DOT:
5101 PROC NEAR
5102 ASSUME DS:ABSO,ES:NOTHING
5103 PUSH DX
5104 SHLD DX,08000H
5105 MOV ES,DX
5106 PUSH AX
5107 PUSH AX
5108 PUSH AX
5109 PUSH AX
5110 PUSH AX
5111 PUSH AX
5112 PUSH AX
5113 CALL WRITE_DOT
5114 PUSH AX
5115 PUSH AX
5116 PUSH AX
5117 PUSH AX
5118 PUSH AX
5119 PUSH AX
5120 PUSH AX
5121 PUSH AX
5122 OR AL,CL ; SAVE DOT VALUE
5123 R1:
5124 MOV ES:[SI],AL ; WRITE DOT
5125 POP AX
5126 JMP V_RET
5127 R2:
5128 XOR AL,CL ; EXCLUSIVE OR THE DOTS
5129 JMP R1 ; FINISH UP THE WRITING
5130 WRITE_DOT:
5131 PROC NEAR
5132 CRP MODE,OFR
5133 JB NO_ADJ2 ; BASE CARD
5134 CALL MEM_DET ; BASE CARD
5135 NO_ADJ2:
5136 AND AL,10000101B ; 85H, XOR C2 CO MASK
5137 MOV AH,AL
5138 OR AL,AH ; EXPAND CO TO C1, C2 TO C3
5139 PUSH AX
5140 PUSH AX ; BUILD ?(80H) + {0,3,C,F}
5141 NO_ADJ2:
5142 PUSH AX ; ROW VALUE
5143 MOV DX,0A000H ; BX-OFFSET, AL-BIT MASK
5144 MOV DH,3 ; BX-OFFSET, AL-BIT MASK
5145 MOV AH,GRAH_ADDR ; GRAPHICS CHIP
5146 OUT_DX ; BIT MASK REGISTER
5147 CALL SET_BIT_MASK ; SET BIT MASK
5148 SRSLDD ES,0A000H ; REGEN SEGMENT
5149 MOV ES,DX ; RECOVER COLOR
5150 POP DX ; SAVE COLOR
5151 MOV CH,AL ; SEE IF XOR
5152 MOV CL,0 ; XOR
5153 MOV AH,0 ; DO XOR
5154 MOV AL,AH ; FUNCTION
5155 CALL SET_REGISTER ; SET THE REGISTER
5156 CALL SKIPL ; SKIP THE BLANK
5157 CALL WRITE_DOT ; WRITE THE DOT
5158 CALL OUT_DX ; SET THE REGISTER
5159
5160 WD_A:
5161 MOV DL,SEQ_ADDR ; ENABLE ALL MAPS
5162 AND AL,0FFH ; SET THE REGISTER
1C91 26: 8A 07
1C92 0D
1C94 26 88 07
1C95 26 88 07
1C96 B4 C4
1C9B B4 02
1C9D 8A 0F
1C9E 8A 0F
1C9F E8 0015 R
1C9F 26 84 07
1C9F F7 FF
1C99 26 88 07
1C9A 26 88 07

5167 C MOV AL, ES:[BX]
5168 C SBB AL, AL
5169 C WO_B:
5170 C MOV DL, SEQ_ADDR
5171 C MOV AH, S_MAP
5172 C AL, CH
5173 C AND AL, 0FFH
5174 C CALL OUT_DX
5175 C MOV AL, ES:[BX]
5176 C MOV AL, 0FFH
5177 C MOV ES:[BX], AL
5178 C MOV AL, 0FFH
5179 C MOV ES:[BX], AL
5180 C ;------ NORMALIZE THE ENVIRONMENT
5181 C
5182 C CALL OUT_DX
5183 C MOV DL, GRAPH_ADDR
5184 C MOV AH, DATA_ROT
5185 C SUB AL, A
5186 C OUT DX
5187 C MOV AH, BIT_MASK
5188 C MOV AL, 0FFH
5189 C CALL OUT_DX
5190 C JM V_RET
5191 C WRITE_DOT_2
5192 C ENDP

1C72
1C72 56
1C73 52
1C74 BA A000
1C77 8C C2
1C79 58
1C7A 58
1C7B 8C C2
1C7D 8B 1600 R
1C7E 89 07
1C7F 89 07
1C80 8B D2
1C81 89 07
1C82 89 07
1C83 89 07
1C84 89 07
1C85 89 07
1C86 89 07
1C87 89 07
1C88 89 07
1C89 89 07
1C8A 89 07
1C8B 89 07
1C8C 89 07
1C8D 89 07
1C8E 89 07
1C8F 89 07
1C90 89 07
1C91 89 07
1C92 89 07
1C93 89 07
1C94 89 07
1C95 89 07
1C96 89 07
1C97 89 07
1C98 89 07
1C99 89 07
1C9A 89 07
1C9B 89 07
1C9C 89 07
1C9D 89 07
1C9E 89 07
1C9F 89 07
1CA0 89 07
1CA1 89 07
1CA2 89 07
1CA3 89 07
1CA4 89 07
1CA5 89 07
1CA6 89 07
1CA7 89 07
1CA8 89 07
1CA9 89 07
1CAA 89 07
1CAB 89 07
1CAC 89 07
1CAD 89 07
1CAE 89 07
1CAF 89 07
1CB0 89 07
1CB1 89 07
1CB2 89 07
1CB3 89 07
1CB4 89 07
1CB5 89 07
1CB6 89 07
1CB7 89 07
1CB8 89 07
1CB9 89 07
1CBA 89 07
1CBB 89 07
1CBC 89 07
1CBD 89 07
1CBE 89 07
1CBF 89 07
1CC0 89 07
1CC1 89 07
1CC2 89 07
1CC3 89 07
1CC4 89 07
1CC5 89 07
1CC6 89 07
1CC7 89 07
1CC8 89 07
1CC9 89 07
1CCA 89 07
1CCB 89 07
1CCC 89 07
1CCD 89 07
1CCE 89 07
1CCF 89 07
1CD0 89 07
1CD1 89 07
1CD2 89 07
1CD3 89 07
1CD4 89 07
1CD5 89 07
1CD6 89 07
1CD7 89 07
1CD8 89 07
1CD9 89 07
1CDA 89 07
1CDB 89 07
1CDC 89 07
1CDD 89 07
1CDE 89 07
1CDF 89 07
1CE0 89 07
1CE1 89 07
1CE2 89 07
1CE3 89 07
1CE4 89 07
1CE5 89 07
1CE6 89 07
1CE7 89 07
1CE8 89 07
1CE9 89 07
1CEA 89 07
1CEB 89 07
1CEC 89 07
1CED 89 07
1CEE 89 07
1CEF 89 07
1CF0 89 07
1CF1 89 07
1CF2 89 07
1CF3 89 07
1CF4 89 07
1CF5 89 07
1CF6 89 07
1CF7 89 07
1CF8 89 07
1CF9 89 07
1CFA 89 07
1CFB 89 07
1CFC 89 07
1CFD 89 07
1CFE 89 07
1CFF 89 07
1D00 89 07

5193 C RD_S PROC NEAR
5194 C ASSUME DS:ABS0
5195 C PUSH AX
5196 C PUSH DX
5197 C SRLOAD DX,0000h
5198 C MOV DX,0000h
5199 C MOV ES,DX
5200 C POP DX
5201 C POP AX
5202 C MOV AX,DX
5203 C CALL DOT_SUP_1
5204 C MOV CH,7
5205 C SUB CH,CL
5206 C SUB DX,DX
5207 C MOV AL,0
5208 C RET
5209 C RD_S ENDP

5210 C RD_1S PROC NEAR
5211 C MOV CL,CH
5212 C MOV AH,0
5213 C PUSH DX
5214 C PUSH AX
5215 C MOV DX,0000h
5216 C MOV DL,GRAPH_ADDR
5217 C CALL OUT_DX
5218 C MOV AH,ES:[BX]
5219 C MOV AH,CL
5220 C SHL AH,1
5221 C AND AH,1
5222 C RET
5223 C RD_1S ENDP

5224 C ;------ READ DOT
5225 C
5226 C AHD:
5227 C ASSUME DS:ABS0
5228 C CMP CRT_MODE,7
5229 C JA R_1
5230 C
5231 C READ_DOT PROC NEAR
5232 C ASSUME DS:ABS0, ES:NOTHING
5233 C PUSH DX
5234 C SRLLOAD ES,0000h
5235 C MOV ES,0000h
5236 C MOV ES,DX
5237 C POP DX
5238 C CALL RD_S
5239 C MOV AL, ES:[SI]
5240 C AND AL, AH
5241 C OR AL, AH
5242 C SHL AL,1
5243 C MOV CL,DH
5244 C MOV AL,DL
5245 C JMP V_RET
5246 C READ_DOT ENDP

5247 C R_1:
5248 C CMP CRT_MODE,0FH
5249 C JB READ_DOT_2
5250 C CALL READ_DOT_2
5251 C JC READ_DOT_2
5252 C
5253 C REAN_DOT_1 PROC NEAR ; 2 MAPS
5254 C ASSUME DS:ABS0, ES:NOTHING
5255 C CALL RD_S
5256 C CALL RD_S
5257 C OR DL,AH
5258 C SHL AH,1
5259 C OR DH,AH
5260 C SHL AH,1
5261 C MOV AL,2
5262 C CALL RD_S
5263 C SHL AH,1
5264 C OR DH,AH
5265 C SHL AH,1
5266 C OR DH,AH
5267 C SHL AH,1
5268 C MOV AL,DL
5269 C JMP V_RET
5270 C REAN_DOT_1 ENDP

5271 C READ_DOT_2 PROC NEAR ; 4 MAPS
5272 C ASSUME DS:ABS0, ES:NOTHING
5273 C CALL RD_S
5274 C CALL RD_S
5275 C CALL RD_2A
5276 C CALL RD_1S
5277 C MOV CL,AL
5278 C SHL AH,1
5279 C OR DH,AH
5280 C INC AL
5281 C CMP AL,3
5282 C JBE RD_2A
5283 C MOV AL,CL
5284 C JMP V_RET
5285 C READ_DOT_2 ENDP

5286 C
5287 C WRITE_TTY WRITE_TELETEYPE TO ACTIVE PAGE
5288 C THIS INTERFACE PROVIDES A TELETEYPE LIKE INTERFACE TO THE VIDEO
5289 C CARD. EACH TIME A CHARACTER IS WRITTEN TO THE CARD, THE CURSOR
5290 C POSITION AND THE INDEX ARE MOVED TO THE NEXT POSITION. IF THE
5291 C CURSOR LEAVES THE LAST COLUMN OF THE FIELD, THE COLUMN IS SET
TO ZERO, AND THE ROW VALUE IS INCREMENTED. IF THE ROW VALUE LEAVES THE FIELD, THE CURSOR IS PLACED ON THE LAST ROW, FIRST COLUMN, AND THE ENTIRE SCREEN IS SCROLLED UP ONE LINE. WHEN THE SCREEN IS SCROLLED UP, THE ATTRIBUTE FOR FILLING THE NEWLY BLANK LINE IS READ FROM THE ATTRIBUTES USED ON THE PREVIOUS LINE BEFORE THE SCROLL, IN CHARACTER MODE. IN GRAPHICS MODE, THE COLOR IS USED.

ENTRY

(AH) = CURRENT CRT MODE

(I) = CHAR TO WRITE

(NOTE THAT BACK SPACE, CAR RET, BELL AND LINE FEED ARE HANDLED AS COMMANDS RATHER THAN AS DISPLAYABLE GRAPHICS.)

(I) IS ALSO USED COLOR FOR CHAR WRITE IF CURRENTLY IN A GRAPHICS MODE

EXIT

ALL REGISTERS SAVED

.quate .usume CS:CODE,DS:ABSO .save registers .mov bh,active_page .get the active page .push bx .save .mov dl,@70h .page to bl .xor bh,bh .clear high byte .sah bx,bl .set 2tor word offset .mov dx,[bx+OFFSET CURSOR_POSN] .pop bx .recover

;----- DX NOW WAS THE CURRENT CURSOR POSITION

mov ah,12h ;is it carriage return .cmp al,0dh .carret .je u7 is line feed .je u10 .is it backspace .cmp al,08h .je u11 .is it a bell .cmp al,07h .je u11 .bell .write char only .je u13 .write char .write char

;----- WRITE THE CHAR TO THE SCREEN

mov ah,10 ;write char only .mov cx,1 .write one char .int 10h .write the char

;----- POSITION THE CURSOR FOR NEXT CHAR

inc dl ,byte ptr crt_cols ;test for column overflow .jnz u7 ;set cursor .sub dl,dl ;column for cursor .jmp u6 ;set_cursor_inc

;----- SCROLL REQUIRED

u1: call set_cpos ;set the cursor

;----- DETERMINE VALUE TO FILL WITH DURING SCROLL

mov al,crt_mode ;get the current mode .cmp al,4 ;read-cursor .jb u2 fill with background .hb bh,bh .cmp al,7 ;scroll-up .jne u3 read-cursor .int 10h ;read/attr .mov bh,bh ;store in bh .scroll up one .mov cx,cx ;scroll one line .mov dh,rows ;upper left corner .mov dl,byte ptr crt_cols ;lower right column .dec dl .

;video-call-return .scroll up the screen .scroll-up retire .restore the character .return to caller .set-cursor-inc .next row .set-cursor

;----- BACK SPACE FOUND

or dl,dl ;already at end of line .jz u7 ;set-cursor .nc -- just move it back .jmp u7 ;set_cursor

;----- CARRIAGE RETURN FOUND

mov dl,@0 ;carriage return .mov bx,0 ;move to first column .mov dh,0 ;set_cursor

;----- LINE FEED FOUND

inc dh,rows ;yes, scroll the screen .jge u9 ;no, just set the cursor

;----- BELL FOUND

mov dl,2 ;set up count for beep .mov bh,0 ;sound the pod bell .int 18h ;tty_return

;----- CURRENT VIDEO STATE

assume ds:abso ;get number of columns .mov bh(byte ptr crt_cols) .and al,080h .or al,crt_mode
1096 5F
1097 5C
1098 59
1099 5A
109A 54
109B 5F
109C 07
109D 50
109E CF
109F 5F
10A0 5C
10A1 59
10A2 50
10A3 FA
10A4 EC
10A5 A8 08
10A6 78
10A7 58
10A8 B2 C0
10A9 EE 00
10AA EE
10AB EE
10AC EE
10AD EE
10AE EE
10AF EE
10B0 EE
10B1 EE
10B2 EE
10B3 EE
10B4 EE
10B5 EE
10B6 EE
10B7 E8 0005 R
10B8 B2 C0
10B9 EE 20
10BA EE
10BB EE
10BC EE
10BD EE
10BE EE
10BF EE
10C0 EE
10C1 EE
10C2 EE
10C3 EE
10C4 EE
10C5 EE
10C6 EE
10C7 EE
10C8 EE
10C9 EE 0005 R
10CA EC
10CB EE
10CC EE
10CD EE
10CE EE
10CF EE
10D0 EE
10D1 EE
10D2 EE
10D3 EE
10D4 EE
10D5 EE
10D6 EE
10D7 EE
10D8 EE
10D9 EE
10DA EE
10DB EE
10DC EE
10DD EE
10DE EE
10DF EE
10E0 EE
10E1 EE
10E2 EE
10E3 EE
10E4 EE
10E5 EE
10E6 EE
10E7 EE
10E8 EE
10E9 EE
10EA EE
10EB EE
10EC EE
10ED EE
10EE EE
10EF EE
10F0 EE
10F1 EE
10F2 EE
10F3 EE
10F4 EE
10F5 EE
10F6 EE
10F7 EE
10F8 EE
10F9 EE
10FA EE
10FB EE
10FC EE
10FD EE
10FE EE
10FF EE
1100 EE
1101 EE
1102 EE
1103 EE
1104 EE
1105 EE
1106 EE
1107 EE
1108 EE
1109 EE
110A EE
110B EE
110C EE
110D EE
110E EE
110F EE
1110 EE
1111 EE
1112 EE
1113 EE
1114 EE
1115 EE
1116 EE
1117 EE
1118 EE
1119 EE
111A EE
111B EE
111C EE
111D EE
111E EE
111F EE
1120 EE
1121 EE
1122 EE
1123 EE
1124 EE
1125 EE
1126 EE
1127 EE
1128 EE
1129 EE
112A EE
112B EE
112C EE
112D EE
112E EE
112F EE
1130 EE
1131 EE
1132 EE
1133 EE
1134 EE
1135 EE
1136 EE
1137 EE
1138 EE
1139 EE
113A EE
113B EE
113C EE
113D EE
113E EE
113F EE
5419 C POP DI
5420 C POP SI
5421 C POP CX
5422 C POP DX
5423 C POP ES
5424 C POP BP
5425 C POP SP
5426 C IRET
5427 C
5428 C SUBTTL
5429 C
5430 C
5431 C
5432 C PAL_SET PROC NEAR
5433 C PUSH AX
5434 C CALL WHAT_BASE
5435 C CL
5436 C
5437 C VR:
5438 C IN AL,DX
5439 C JZ VERT
5440 C POP AX
5441 C MOV DL,ATTR_WRITE
5442 C XCHG DX,AL
5443 C OUT DX,AL
5444 C XCHG AL,AH
5445 C OUT DX,AL
5446 C MOV AL,020H
5447 C OUT DX,AL
5448 C STI
5449 C RET
5450 C FAL_SET ENDP
5451 C
5452 C PAL_ON PROC NEAR
5453 C CALL PAL_INIT
5454 C MOV DL,ATTR_WRITE
5455 C MOV AH,00H
5456 C OUT DX,AL
5457 C RET
5458 C PAL_ON ENDP
5459 C
5460 C PAL_INIT PROC NEAR
5461 C CALL WHAT_BASE
5462 C INT AL,DX
5463 C RET
5464 C PAL_INIT ENDP
5465 C
5466 C ;------ SET PALETTE REGISTERS
5467 C
5468 C AM10:
5469 C ASSUME DS:AB80
5470 C TEST INFO,2
5471 C JNZ BM_1
5472 C ; IN MONOCHROME MODE
5473 C
5474 C ;----- HERE THE EGA IS IN A COLOR MODE
5475 C CMP SYST_PTR ADOR_6845,08AH
5476 C JE BM_OUT
5477 C BM_OK:
5478 C MOV AH,AL
5479 C OR AH,AH
5480 C JNZ BM_1
5481 C
5482 C ;----- SET INDIVIDUAL REGISTER
5483 C
5484 C SUB BF,BP
5485 C LES DI,SAVE_PTR
5486 C ADD DI,0
5487 C LES DI,DWORD PTR ES:[DI]
5488 C MOV AX,ES:[DI]
5489 C OR AH,AH
5490 C JZ TLO_1
5491 C INC BP
5492 C TLO_1:
5493 C
5494 C CALL PAL_INIT
5495 C MOV AH,BL
5496 C MOV AL,BH
5497 C CALL PAL_SET
5498 C CALL PAL_ON
5499 C OR BM_OUT
5500 C JZ BM_OUT
5501 C MOV AL,BH
5502 C SUB DI,8X
5503 C ADD DI,8X
5504 C MOV ES:[DI],AL
5505 C BM_OUT:
5506 C JMP Y_RET
5507 C
5508 C BM_1:
5509 C DEC AH
5510 C JNZ BM_2
5511 C
5512 C SUB BP,BF
5513 C LES DI,SAVE_PTR
5514 C ADD DI,0
5515 C LES DI,DWORD PTR ES:[DI]
5516 C MOV AX,ES:[DI]
5517 C OR AH,AH
5518 C JZ TLO_2
5519 C INC BP
5520 C TLO_2:
5521 C
5522 C ;----- SET 9VSCANS REGISTER
5523 C
5524 C CALL PAL_INIT
5525 C MOV AH,01H
5526 C MOV AL,BH
5527 C CALL PAL_SET
5528 C CALL PAL_ON
5529 C OR BM_OUT
5530 C JZ BM_OUT
5531 C ADD DI,01H
5532 C MOV ES:[DI],DH
5533 C
5534 C JMP V_RET
5535 C
5536 C
5537 C BM_2:
5538 C DEC AH
5539 C JNZ BM_3
5540 C
5541 C ;----- SET 16 PALETTE REGISTERS AND OVERSCAN REGISTER
5542 C
5543 C PUSH DS
5544 C PUSH ES
1E40 C4 3E 04A8 R      5545    LES   DF_SAVE_PTR
1E44 B3 CF 04           5546    ADD   DI,4
1E47 E9 0C 3D           5548    LES   DI,WORD PTR ES:[DI] ; ES:DI PTR TO PAL SAVE AREA
1E4A 8C CO              5549    MOV   AX,DI
1E4C 0B 07             5550    OR    AX,DI
1E4E 5551              5551    JZ    TLO_3
1E50 1F                5552    P3P   DS
1E51 1E                5553    PUSH  DS
1E52 8B F2             5554    MOV   SI,DX ; PARAMETER ES
1E54 B9 0011            5555    MOV   CX,17D ; PARAMETER OFFSET
1E57 F3 /A4             5556    REP   MOVSB
1E59 5557              5557    REP   MOVSB
1E5A 5558              5558    TLO_3:
1E5B 07                5559    POP   ES
1E5C 1F                5560    POP   DS
1E5D 5561              5561    CALL  PAL_SET
1E5E 8B 00              5562    MOV   BX,BX
1E60 8B 00              5563    MOV   AH,AH
1E62 5564              5564    CALL  PAL_INIT
1E63 8B 00              5565    MOV   BX,BX
1E65 5566              5566    SUB   AH,AH
1E66 26: 8A 07          5567    MOV   AL,ES:[BX]
1E69 EB 1D9F R          5568    CALL  PAL_SET
1E6C 8B 00              5569    MOV   BX,BX
1E6E 5570              5570    INC   BX
1E6F 5571              5571    CMP   AH,010H
1E70 75 24             5572    JB    24
1E72 FE 04              5573    INC   AH
1E74 5574              5574    MOV   AL,ES:[BX]
1E75 EB 1D9F R          5575    CALL  PAL_SET
1E78 8B 00              5576    MOV   BX,BX
1E7A 5577              5577    CALL  PAL_ON
1E7B 5578              5578    JMP   V_RET
1E7C 5579              5579    BM_3:
1E7E FE CC              5580    DEC   AH
1E80 75 29             5581    JNZ   BM_4
1E82 5582              5582    ;------ TOGGLE INTENSITY/BLINKING BIT
1E83 5583              5583    PUSH  BX
1E84 5584              5584    CALL  MAKE_BASE
1E85 5585              5585    ADD   BX,010H LN_4
1E87 5586              5586    AND   AL,0F7H
1E88 5587              5587    MOV   AL,ES:[BX]
1E89 5588              5588    POP   BX
1E8A 5589              5589    POP   BL
1E8B 5590              5590    OR    BL,BL
1E8C 5591              5591    JNZ   BM_6
1E8D 5592              5592    ;------ ENABLE INTENSITY
1E8E 5593              5593    PUSH  BX
1E8F 5594              5594    CALL  CRT_MODE_SET,1101111B
1E90 5595              5595    AND   AL,0F7H
1E91 5596              5596    JM    BM_7
1E92 5597              5597    BM_6:
1E93 5598              5598    DEC   BL
1E94 5599              5599    JNZ   BM_7
1E95 559A              559A    ;------ ENABLE BLINK
1E96 559B              559B    OR    CRT_MODE_SET,020H
1E97 559C              559C    OR    AL,08H
1E98 559D              559D    BM_7:
1E99 559E              559E    MOV   AH,P_MODE
1E9A 559F              559F    CALL  PAL_SET
1E9B 55A0              55A0    CALL  PAL_SET
1E9C 55A1              55A1    JMP   V_RET
1E9D 55A2              55A2    INCLUDE  VCHGEN.INC
1E9E 55A3              55A3    SUBTL  VCHGEN, INC
1E9F 55A4              55A4    PAGE
1EA0 55A5              55A5    ENTRY
1EA1 55A6              55A6    AL = 0 USER SPECIFIED FONT
1EA2 55A7              55A7    0 = 8 X 8 DOUBLE DOT
1EA3 55A8              55A8    2 = 8 X 8 DOUBLE DOT
1EA4 55A9              55A9    BL = BLOCK TO LOAD
1EA5 55AA              55AA    CH_GEN:
1EA6 55AB              55AB    PUSH  AX ; SAVE THE INVOLVED REGS
1EA7 55AC              55AC    PUSH  BP
1EA8 55AD              55AD    PUSH  CX
1EA9 55AE              55AE    PUSH  DX
1EAA 55AF              55AF    PUSH  ES
1EAB 55B0              55B0    ASSUME DS:ALSO
1EAC 55B1              55B1    CALL  DOS
1EAD 55B2              55B2    MOV   AL,CRT_MODE ; SET DATA SEGMENT
1EAE 55B3              55B3    CALL  CRT_MODE ; GET THE CURRENT MODE
1EB0 55B4              55B4    PUSH  AX ; SAVE THIS
1EB1 55B5              55B5    CMP   AL,7 ; IS THIS MONOCHROME
1EB2 55B6              55B6    JE    H14 ; MONOCHROME VALUES
1EB3 55B7              55B7    MOV   AL,MODE_OBH ; COLOR VALUES
1EB4 55B8              55B8    JMP   SHORT H15 ; SKIP
1EB5 55B9              55B9    H14: MOV   CRT_MODE,OCH ; MONOCHROME VALUES
1EB6 55BA              55BA    H15: MOV   CRT_MODE,OCH ; MONOCHROME VALUES
1EB7 55BB              55BB    CALL  DOS ; RESET THE DATA SEGMENT
1EB8 55BC              55BC    CALL  DOS ; RECOVER OLD MODE VALUE
1EB9 55BD              55BD    MOV   CRT_MODE,AL ; TURN OFF LOW MEMORY
1EBA 55BE              55BE    POP   ES ; RESTORE REGS THAT WERE
1EBB 55BF              55BF    POP   DX ; USED BY THE MODE SET
1EBC 55C0              55C0    POP   CX ; ROUTINES
1EBD 55C1              55C1    POP   BP
1EBE 55C2              55C2    POP   AX ; SET FLAGS
1EBF 55C3              55C3    OR    AL,AL ; USER SPECIFIED FONT
1EC0 55C4              55C4    JZ    DO_MAP2 ; SET SEGMENT TO
1EC1 55C5              55C5    PUSH  CS ; THIS MODULE
1EC2 55C6              55C6    PUSH  ES ; DO NOT START OFFSET
1EC3 55C7              55C7    SLL   DX,DX ; CHAR COUNT (FULL SET)
1EC4 55C8              55C8    MOV   CX,0256B ; WHICH CHARACTER
1EC5 55C9              55C9    MOV   DX,0 ; MUST BE ON
1EC6 55CA              55CA    MOV   BH,01DH ; BYTES PER CHARACTER
1EC7 55CB              55CB    MOV   BH,01DH ; 8 X 8 TABLE OFFSET
1EC8 55CC              55CC    MOV   BH,01DH ; STORE IT
1EC9 55CD              55CD    SHORT DO_MAP2:
1ECA 55CE              55CE    H7: MOV   BH,8 ; 8 X 8 FONT
1ECB 55CF              55CF    MOV   BP,OFFSET CGDDOT ; ROM 8 X 8 DOUBLE DOT
1ECC 55D0              55D0    ; ALPHA CHARACTER GENERATOR LOAD :
C ENTRY
ES:BP = POINTER TO TABLE
CX = COUNT OF CHARACTERS
DX = COUNT OF BLOCKS INTO MAP 2
BH = BYTES PER CHARACTER
BL = MAP 2 BLOCK TO LOAD

DO_MAP2:
PUSH ES
POP DS
MOV DX, OAO00H
SAVE REGISTER
ADDRESSING TO MAP 2
RECOVER REGISTER
MULTIPLY BY 020H SINCE
MAP 2 BLOCKS PER
CHARACTER IS 320=020H
RECOVER
SHL DX, CL
SHL BL,BL
BLOCK ZERO
JZ H3
INC TO NEXT BLOCK
ANY MORE
DO ANOTHER
BYTES PER CHARACTER
ZERO
OFFSET INTO MAP
OFFSET INTO TABLE
CHARACTER COUNT
NEXT CHARACTER POSITION
RECOVER CHARACTER COUNT
DO THE REST
SAVE CHARACTER COUNT
ONE ENTIRE CHARACTER
A-1
ADJUST OFFSET
NEXT CHARACTER POSITION
RECOVER CHARACTER COUNT
DO THE REST

LO_OVER:
RET

BPK_1:
ASSUME DS:ABSO
CALL DBPK
SET LOW MEMORY SEGMENT
SET BYTES/CHARACTER
CRTC REGISTER
MOV POINTS,AX
MOV DX,ADDR_6845
CRU_MODE,7
JNE H11A
MOV AH,C_UNDERLN_LOC
OUT_OX
SET THE UNDERLINE LOC
DEC AL
POINTS - 1
ROPH
SET THE CHARACTER HEIGHT
POINTS - 2
CALL OUT_OX
CURSOR START
CURSOR END
ADJUST END
SET C, A, B'S05 CALL
SET THE CURSOR
MOV BL,CRT_MODE
GET THE CURRENT MODE
MAX SCANS ON SCREEN
MAX SCANS ON ALPHA MODE
MUST BE 250
MOV AX,250D
SET FOR 200
DIV POINTS
PREPARE TO DIVIDE
MAX ROWS ON SCREEN
ADD ROWS
SAVE ROWS
READJUST
CLEAR
ROWS*BYTES/CHAR
CRTC ADDRESS
SCANS DISPLAYED
SET
SET CHARACTER ROWS
ADJUST
PROMOULS
#2 FOR ALPHA MODE
SPACE BETWEEN PAGES
BYTES PER LINE
VIDEO ON
RETURN TO CALLER

------ LOADABLE CHARACTER GENERATOR ROUTINES

AH11:
CMP AL,010H
CHECK PARAMETER
JAE AH11_ALPHA1
NEXT STAGE

------ ALPHA MODE ACTIVITY HERE

CMP AL,03H
RANGE CHECK
JAE H1
NEXT STAGE
CAL CH_GEN
SET THE CHAR GEN
CAL SET_PEGS
VIDEO ON
ASSUME DS:ABSO
CAL
SET THE DATA SEGMENT
GET THE MODE
GET THE TYPE
EMULATE CORRECT CURSOR
RETURN TO CALLER

------ SET THE CHARACTER GENERATOR BLOCK SELECT REGISTER

N1:
JNE H2
NOT IN RANGE
MOV DH,3
SEQUENCER
MOV DL,SEQ_ADDR
AH=S_RESET, AL=1
CHAR BLOCK REGISTER
SET THE VALUE
SET
1FCA BB 00D3
1FCD EB 0015 R
1FDD 5797 C MOV AX, 3 ; AH=S_RESET, AL=3
1FDD 5798 C CALL OUT_DX
1FDD 5799 C HE2:
1FDD E9 219E R 5800 JMP V_RET ; RETURN TO CALLER
1FDD 5802 AH11_ALPHA:
1FDD 5803 ASSUME DS:ABSO
1FDD 5804 CMP AL, 020H
1FDD 5805 JAE AH11_GRAPHICS
1FDD 5806
1FDD 5807 ;------ ALPHA MODE ACTIVITY HERE
1FDD 5808
1FDD 5809 SUB AL, 010H ; ADJUST TO 0 - N
1FDD 5810 CMP AL, 02H ; INVALID CHECK
1FDD 5811 JA F11 ; SAVE
1FDD 5812 PUSH AX
1FDD 5813 PUSH BX
1FDD 5814 CALL CH_GEN ; LOAD THE CHAR GEN
1FDD 5815 CALL SET_REGS
1FDD 5816 POP BX
1FDD 5817 POP AX
1FDD 5818 MOV AH, AL
1FDD 5819 OR AH, AH
1FDD 5820 MOV AL, BH
1FDD 5821 JZ F11 ; DO NOT SET 8 BYTES/CHAR
1FDD 5822 MOV AL, 8 ; 8 X 8 FONT
1FDD 5823 CMP AH, 1 ; IS THE CALL FOR MONOC
1FDD 5824 JNE H13 ; NO, SO SET AT 8
1FDD 5825 MOV AL, 14d ; MONOC SET
1FDD 5826 H13:
1FDD 5827 SUB AH, AH ; CLEAR UPPER BYTE
1FDD 5828 JMP BRK_1 ; CONTINUE
1FDD 5829
1FDD 5830 ;------ GRAPHICS MODE ACTIVITY HERE
1FDD 5831 AH11_GRAPHICS:
1FDD 5832 ASSUME DS:ABSO
1FDD 5833 CMP AL, 020H ; RANGE CHECK
1FDD 5834 JAE AH11_INFORM
1FDD 5835 SUB AL, 020H
1FDD 5836 JNZ F11
1FDD 5837
1FDD 5838 ;------ COMPATIBILITY, UPPER HALF GRAPHICS CHARACTER SET
1FDD 5839
1FDD 5840 ASSUME DS:ABSO
1FDD 5841 SRLOAD
1FDD 5842 SUB DX, DX
1FDD 5843 PUSH DX
1FDD 5844 MOV DS, DX
1FDD 5845 CLC
1FDD 5846 MOV WORD PTR EXT_PTR , BP
1FDD 5847 MOV WORD PTR EXT_PTR + 2 , ES
1FDD 5848 STI
1FDD 5849 F11:
1FDD 5850 JMP V_RET
1FDD 5851 F10:
1FDD 5852 ASSUME DS:ABSO
1FDD 5853 PUSH DX
1FDD 5854 SRLOAD
1FDD 5855 SUB DX, DX
1FDD 5856 PUSH DX
1FDD 5857 CMP AL, 03H ; RANGE CHECK
1FDD 5858 JA F11
1FDD 5859 DEC AL
1FDD 5860 JZ F19
1FDD 5861 PUSH CS
1FDD 5862 PUSH ES
1FDD 5863 DEC AL
1FDD 5864 JZ F13
1FDD 5865 MOV CX, 14d
1FDD 5866 MOV BX, OFFSET CGMN ; ROM 8 x 14 CHARACTER SET
1FDD 5867 MOV SHORT F13
1FDD 5868
1FDD 5869 F13:
1FDD 5870 MOV CX, B
1FDD 5871 MOV BP,OFFSET C300DOT ; ROM 8 x 8 DOUBLE DOT
1FDD 5872 F19:
1FDD 5873 CLI
1FDD 5874 MOV WORD PTR GRX_SET , BP
1FDD 5875 MOV WORD PTR GRX_SET + 2 , ES
1FDD 5876 STI
1FDD 5877 ASSUME DS:ABSO
1FDD 5878 CALL POINTS,CX
1FDD 5879 MOV AL, BL
1FDD 5880 MOV BX, OFFSET RT
1FDD 5881 OR AL, AL
1FDD 5882 JNZ DR-1
1FDD 5883 MOV DL, AL
1FDD 5884 JMP DR-1
1FDD 5885 DR-3:
1FDD 5886 CMP AL, 3
1FDD 5887 JBE DR-2
1FDD 5888 MOV AL, 2
1FDD 5889 DR-2:
1FDD 5890 DR-1:
1FDD 5891 XLAT
1FDD 5892 CS:RT
1FDD 5893 DEC AL
1FDD 5894 MOV ROWS,AL
1FDD 5895 JMP V_RET
1FDD 5896
1FDD 5897 RT LABEL BYTE
1FDD 5898 DB 00D,140,25D,43D
1FDD 5899
1FDD 5900 ;------ INFORMATION RETURN DONE HERE
1FDD 5901
1FDD 5902
1FDD 5903 AH11_INFORM:
1FDD 5904 ASSUME DS:ABSO
1FDD 5905 CMP AL, 030H
1FDD 5906 JE F6
1FDD 5907 F5:
1FDD 5908 F6:
1FDD 5909 MOV CX, POINTS
1FDD 5910 MOV DL, ROWS
1FDD 5911 CMP DH, 7
1FDD 5912 JA F9
1FDD 5913 CMP Bh, 1
1FDD 5914 JZ F7
1FDD 5915
1FDD 5916 ASSUME DS:ABSO
1FDD 5917 PUSH DS
1FDD 5918 PUSH DX
1FDD 5919 SRLOAD
1FDD 5920 SUB DS, DX
1FDD 5921 MOV DS, DX
1FDD 5922 POP DX
208A 04 FF
208C 75 07
208E 2E 007C R
2091 8B 90
2093 C9
2095 C9 E1 010C R
2099 EB 13 96

5923 C OR BH,BH
5924 F9 F9
5925 C LES BP,EXT_PTR
5926 C JMP INFO_OUT
5927 C F9:
5928 C LES BP,ORX_SET
5929 C JMP INFO_OUT
5930 C
5931 C ;----- HANDLE BH = 2 THRU BH = 5 HERE RETURN ROM TABLE POINTERS
5932 C
5933 C F7:
5934 C ASSUME DS:ABS0
5935 C SUB BH,2
5936 C MOV BL,BH
5937 C SUB BH,BH
5938 C SAL BX,1
5939 C ADD BX,[OFFSET TBL_5]
5940 C MOV BP,CS:[BX]
5941 C PUSH BP
5942 C POP ES
5943 C
5944 C INFORM_OUT:
5945 C POP DI
5946 C POP SI
5947 C POP BX
5948 C POP AX
5949 C POP DS
5950 C POP AX
5951 C POP AX
5952 C POP AX
5953 C IRET
5954 C
5955 C ;----- TABLE OF CHARACTER GENERATOR OFFSETS
5956 C
5957 C TBL_5 LABEL WORD
5958 C DW OFFSET CGMN
5959 C DW OFFSET CGDDOT
5960 C DW OFFSET CGMNTF
5961 C DW OFFSET CGMNFEG
5962 C
5963 C SUBTTL
5964 C
5965 C ;----- ALTERNATE SELECT
5966 C
5967 C AH12:
5968 C ASSUME DS:ABS0
5969 C CMP BL,010H ; RETURN ACTIVE CALL
5970 C JBE ACT_1
5971 C JE ACT_3
5972 C CMP BL,020H ; ALTERNATE PRINT SCREEN
5973 C JEC V_RET
5974 C JMP V_RET ; INVALID CALL
5975 C ACT_2:
5976 C SRLOAD DS,0
5977 C SUB DX,DX
5978 C MOV WORD PTR INT5_PTR,OFFSET PRINT_SCREEN
5979 C MOV WORD PTR INT5_PTR+2,CS
5980 C JMP V_RET ; NEW PRINT SCREEN
5981 C
5982 C ACT_3:
5983 C MOV BN,INFO ; LOOKING FOR MONOG BIT
5984 C AND BN,BH ; ISOLATE
5985 C SHR BH,1 ; ADJUST
5986 C
5987 C AND AL,01000000B ; LOOKING FOR MEMORY
5988 C SHL AL,3 ; MEMORY BITS
5989 C SHL AL,1 ; SINGLE BIT
5990 C SHL AL,1 ; ADJUST MEN VALUE
5991 C MOV BL,AL ; RETURN REGISTER
5992 C
5993 C MOV CL,INFO_3 ; FEATURE/SWITCH
5994 C MOV CH,CL ; DIALOGUE BIT CH
5995 C AND CL,0FH ; MASK OFF SWITCH VALUE
5996 C SHR CH,1 ; MOVE FEATURE VALUE
5997 C SHR CH,1
5998 C SHR CH,1
5999 C ANP CH,0FH ; MASK IT
6000 C
6001 C POP DI
6002 C POP DX
6003 C POP BX
6004 C POP DX ; DISCARD BX
6005 C POP DX ; DISCARD CX
6006 C POP DX
6007 C POP DX
6008 C POP DS
6009 C POP PS
6010 C POP FS
6011 C POP BP
6012 C IRET
6013 C AH12_X:
6014 C JMP V_RET ; RETURN TO CALLER
6015 C ACT_1:
6016 C STR_OUTZ;
6017 C JMP V_RET ; RETURN TO CALLER
6018 C
6019 C ;----- WRITE STRING
6020 C
6021 C AH13:
6022 C CMP AL,00 ; RANGE CHECK
6023 C JAE STR_OUTZ ; INVALID PARAMETER
6024 C CKZ STR_OUTZ
6025 C PUSH BX ; SAVE REGISTER
6026 C MOV BL,BH ; GET PAGE TO LOW BYTE
6027 C SUB BH,BH
6028 C SAL BX,1 ; #2 FOR WORD OFFSET
6029 C MOV SI,[BX + OFFSET CURSOR_POSN] ; GET CURSOR POSITION
6030 C PUSH SI ; RESTORE
6031 C PUSH SI ; CURRENT VALUE ON STACK
6032 C PUSH AX
6033 C NOV AX,0200H ; SET THE CURSOR POSITION
6034 C INT 10
6035 C POP AX
6036 C POP AX
6037 C POP AX
6038 C PUSH CX
6039 C PUSH BX
6040 C PUSH DS
6041 C XCHG AH,AL ; AH,ES:[BP]
6042 C INC BP ; SET THE CHAR TO WRITE
6043 C CMP AH,0AH ; CARRIAGE RETURN
6044 C JE STR_CR_LF ; LINE FEED
6045 C CMP AH,08H ; BACKSPACE
2146   3h   35                      6099      JF     STR_CR_LF                ; BELL
2148   3c   07                      609a      AL     OTH
214a   4b   31                      609b      JE     STR_CR_LF
214c   ac   0c                       609c      MOV    CHR
214e   ac   0f                       609d      MOV    AN_2
2150   4f   02                      609e      CMPO   AN_2
2152   52   02                      609f      JB     DO_STR
2154   4c   02 3a 5e 00               60a0      PUSH   BP
2158   45                           60a1      INC    BP
2159   e2   aa                       60a2      MOV    [BP+3CH],BP
215b   e6                           60a3      INC    BP
215c   8c   02                       60a4      MOV    DL,COL
215e   de   ee                       60a5      MOV    AX,0E00H
2160   ed   ee                       60a6      OR     CHR,AL
2162   8c   02                       60a7      MOV    DL,COL
2163   db   ee                       60a8      DECB   CHR
2164   cc                           60a9      INT    10H
2165   03   0C                       60aa      INC    CHR
2166   ea   ea                       60ab      JP     SHORT _STRING_4
2167   8c   0a                       60ac      MOV    DX,STR_
2168   bd   ee                       60ad      SUB    CX,COL
2169   ec   ee                       60ae      OR     DL,COL
216a   ea   ea                       60af      POP    BX
216b   b1   01                       60b0      SHL    CX,1
216c   ea   ea                       60b1      POP    BX
216d   da   ee                       60b2      SHR    DX,1
216e   bd   ee                       60b3      SUB    CX,COL
216f   b8   ee                       60b4      MOV    BL,BX
2170   ea   ea                       60b5      POP    BX
2171   da   ee                       60b6      SHR    DX,1
2172   8c   02                       60b7      MOV    AX,DWORD PTR [BX+CX]
2173   b9   cf 02 00 50               60b8      MOV    AH,ODH
2177   8b   c2                       60b9      MOV    AL,10H
2178   8d   0c                       60ba      ADD    BX,CX
2179   01   01                       60bb      ADD    BL,AL
217a   c8                           60bc      ret
217b   ba   cf 02 00 50               60bd      mov    dx,dword ptr [bx+cx]
217f   8b   c6                       60be      mov    ax,[dx]
2180   8d   0c                       60bf      add    bx,cx
2181   01   01                       60c0      add    bl,al
2182   c8                           60c1      ret

152 IBM Enhanced Graphics Adapter August 2, 1984
21DE 75 02        6175 C    JNZ PR115 ; JUMP IF VALID CHAR
21ED B8 20         6176 C    MOV AL,' ' ; MAKE A BLANK
21EC 52            6177 C    PUSH DX ; SAVE CURSOR POSITION
21ED 52            6178 C    XOR DX,DX ; INDICATE PRINTING
21EE 33 D2         6179 C    XOR AH,AH ; FICTITIOUS PRINT CHAR IN [AL]
21EF 84 E2         617A C    INT 17H ; PAINT THE CHARACTER
21F0 CD 17         617B C    POP DX ; RECALL CURSOR POSITION
21F1 5A            617C C    POP DL ; FOR PRINTER ERROR
21F2 8A 29         617D C    CMP CL,DL ; JUMP IF ERROR DETECTED
21F3 75 21         617E C    JNZ ERR10 ; ADJUST COLUMN
21F4 8A 1C         617F C    CMP CL,DL ; SEE IF AT END OF LINE
21F5 75 0F         6180 C    JNZ PR110 ; IF NOT PROCEED
21F6 8A 02         6181 C    CMP CL,DL ; SEE IF AT COLUMN 0
21F7 8A 02         6182 C    CMP CL,DL ; [AH]=0
21F8 8A 02         6183 C    CMP CL,DL ; SAVE NEXT CURSOR POSITION
21F9 8A 02         6184 C    CMP CL,DL ; USETED CARRIAGE RETURN
21FA 8A 02         6185 C    CMP CL,DL ; RECALL CURSOR POSITION
21FB 8A 02         6186 C    CMP CL,DL ; GO TO NEXT LINE
21FC 8A 02         6187 C    CMP CL,DL ; FINISHED?
21FD 8A 02         6188 C    CMP CL,DL ; IF NOT CONTINUE
21FE 8A 02         6189 C    CMP CL,DL ; RECALL CURSOR POSITION
21FF 8A 02         618A C    CMP CL,DL ; TO REQUEST CURSOR SET REQUEST
2200 8A 02         618B C    CMP CL,DL ; CURSOR POSITION RESTORED
2201 8A 02         618C C    CMP CL,DL ; INDICATE FINISHED
2202 8A 02         618D C    CMP CL,DL ; EXIT THE ROUTINE
2203 8A 02         618E C    CMP CL,DL ; GET CURSOR POSITION
2204 8A 02         618F C    CMP CL,DL ; REQUEST CURSOR SET
2205 8A 02         6190 C    CMP CL,DL ; CURSOR POSITION RESTORED
2206 8A 02         6191 C    CMP CL,DL ; INDICATE ERROR
2207 8A 02         6192 C    CMP CL,DL ; RESTORE ALL THE REGISTERS USED
2208 8A 02         6193 C    CMP CL,DL ; PRINT_SCREEN
2209 8A 02         6194 C    CMP CL,DL ; END
220A 8A 02         6195 C    CMP CL,DL ; -------- CARriage Return, Line Feed Subroutine
220B 8A 02         6196 C    CMP CL,DL ; CR/LF PROC NEAR
220C 8A 02         6197 C    CMP CL,DL ; WILL NOW SEND INITIAL CR, LF
220D 8A 02         6198 C    CMP CL,DL ; TO PRINTER
220E 8A 02         6199 C    CMP CL,DL ; SEND THE LINE FEED
220F 8A 02         619A C    CMP CL,DL ; NOW FOR THE CR
2210 8A 02         619B C    CMP CL,DL ; SEND THE CARriage Return
2211 8A 02         619C C    CMP CL,DL ; END
2212 8A 02         619D C    CMP CL,DL ; CODE ENDS
2213 8A 02         619E C    CMP CL,DL ; END

PAGE 120
SUBTTL MONOCHROME CHARACTER GENERATOR
CODE SEGMENT PUBLIC COM
CCMN LABEL BYTE
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BW 8*14 PATTERN
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; TOP_HALF_F_00
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BOTTOM_HALF_F_00
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_01
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_02
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_03
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_04
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_05
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_06
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_07
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_08
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_09
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_10
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_11
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_12
0000 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ; BT_13
07D2 38 6C 38 00 38 6C 44b
07DA C6 C6 00 00 00 445
07E0 18 30 00 FE 66 448
07E6 7C 00 00 00 449
07E8 00 00 00 CC 76 450
07EE 00 00 00 00 451
07F6 DB 6E 00 00 00 453
07FC 00 3E 6C CC 454
0800 00 00 00 00 455
0804 CC CC 00 00 00 456
080A 18 38 6C 00 00 457
0812 C6 C6 7C 00 00 459
0818 00 00 00 00 460
0820 C6 C6 00 00 00 461
0826 C6 7C 00 00 00 462
082C 00 00 00 00 00 463
0832 C6 C6 38 00 00 464
0838 00 38 00 00 00 465
083E 00 38 7C 00 00 466
0844 00 38 7C 00 00 467
084A 76 00 00 00 00 468
084E 00 30 18 00 00 469
0854 CC CC 76 00 00 470
085A 00 00 C6 C6 00 471
0860 00 00 C6 C6 00 472
0866 00 00 C6 38 6C 00 473
0872 00 00 C6 38 00 00 474
0878 00 00 C6 C6 00 00 475
0884 00 00 C6 38 00 00 476
088A 00 00 C6 C6 00 00 477
0890 00 00 C6 C6 00 00 478
0896 00 00 C6 C6 00 00 479
08A2 00 00 C6 C6 00 00 480
08A8 00 00 C6 38 00 00 481
08B4 00 00 C6 38 00 00 482
08BC 00 00 C6 38 00 00 483
08C2 00 00 C6 38 00 00 484
08C8 00 00 C6 38 00 00 485
08D4 00 00 C6 38 00 00 486
08DC 00 00 C6 38 00 00 487
08E4 18 18 00 00 00 488
08EA 00 F8 CC FC C4 490
08F0 00 00 00 00 00 491
08F6 CC C6 00 00 00 492
08FC 00 18 18 18 18 493
0902 18 18 18 18 00 494
0908 18 18 18 18 00 495
0914 00 18 30 6C 00 496
091A 00 18 30 6C 00 497
0926 00 18 30 6C 00 498
092C 00 18 30 6C 00 499
0932 00 18 30 6C 00 500
0938 00 18 30 6C 00 501
0944 00 18 30 6C 00 502
094A 00 18 30 6C 00 503
0956 00 18 30 6C 00 504
095C 00 18 30 6C 00 505
0962 00 18 30 6C 00 506
0968 00 18 30 6C 00 507
0974 00 18 30 6C 00 508
097A 00 18 30 6C 00 509
0986 00 18 30 6C 00 510
098C 00 18 30 6C 00 511
0998 00 18 30 6C 00 512
099E 00 18 30 6C 00 513
09A4 00 18 30 6C 00 514
09AC 00 18 30 6C 00 515
09B0 00 18 30 6C 00 516
09B6 00 18 30 6C 00 517
09BC 00 18 30 6C 00 518
09C2 00 00 00 00 00 519
09C8 00 00 30 30 30 520
09D4 00 00 00 00 00 521
09DA 00 00 00 00 00 522
09E6 C6 C6 7C 00 00 523
09EC 00 00 00 00 00 524
0A02 00 00 00 00 00 525
0A08 00 00 00 00 00 526
0A0E 00 00 00 00 00 527
0A14 00 00 00 00 00 528
0A1A 00 00 00 00 00 529
0A26 00 00 00 00 00 530
0A2C 00 00 00 00 00 531
0A32 00 00 00 00 00 532
0A38 00 00 00 00 00 533
0A3E 00 00 00 00 00 534
0A44 00 00 00 00 00 535
0A4A 00 00 00 00 00 536
0A56 00 00 00 00 00 537
0A5C 00 00 00 00 00 538
0A62 00 00 00 00 00 539
0A68 00 00 00 00 00 540
0A74 00 00 00 00 00 541
0A7A 00 00 00 00 00 542
0A86 00 00 00 00 00 543
0A8C 00 00 00 00 00 544
0A98 11 44 11 44 11 44 545
0AA4 11 44 11 44 11 44 546
0AAE 55 AA 55 AA 55 AA 547
0ABE 55 AA 55 AA 55 AA 548
0B06 55 AA 55 AA 55 AA 549
0B12 DD 77 DD 77 DD 550
0B18 DD 77 DD 77 DD 551
0B24 DD 77 DD 77 DD 552
0B30 DD 77 DD 77 DD 553
0B36 DD 77 DD 77 DD 554
0B42 DD 77 DD 77 DD 555
0B48 DD 77 DD 77 DD 556
0B54 DD 77 DD 77 DD 557
0B60 DD 77 DD 77 DD 558
0B66 DD 77 DD 77 DD 559
0B72 DD 77 DD 77 DD 560
0B78 DD 77 DD 77 DD 561
0B84 DD 77 DD 77 DD 562
0B90 DD 77 DD 77 DD 563
0B96 DD 77 DD 77 DD 564
0BA2 DD 77 DD 77 DD 565
0BA8 DD 77 DD 77 DD 566
0BB4 DD 77 DD 77 DD 567
0BBA DD 77 DD 77 DD 568
0BC6 DD 77 DD 77 DD 569

DB 038H,06CH,038H,000H,038H,06CH,06CH,06H : TH_8F
DB 0FEH,06CH,06CH,000H,000H,000H : BT_8F
DB 018H,039H,060H,000H,0FEH,066H,060H,07CH : TH_90
DB 060H,066H,0FEH,000H,000H,000H : TH_90
DB 000H,000H,000H,000H,0CCH,076H,03EH,07EH : TH_91
DB 0D8H,0D8H,0E6H,000H,000H,000H : TH_91
DB 000H,000H,03EH,06CH,0CCH,0CCH,0FEH,0CCH : TH_92
DB 0CCC,0CCH,0E6H,000H,000H,000H : TH_92
DB 000H,010H,038H,0CCH,000H,07CH,0C6H,0C6H : TH_93
DB 0C6H,0C6H,07CH,000H,000H,000H : TH_93
DB 000H,000H,0C6H,0C6H,0C6H,0C6H,0C6H,0C6H : TH_94
DB 0C6H,0C6H,07CH,000H,000H,000H : TH_94
DB 000H,010H,038H,0CCH,000H,07CH,0C6H,0C6H : TH_95
DB 0C6H,0C6H,07CH,000H,000H,000H : TH_95
DB 000H,030H,078H,0CCH,000H,0CCH,0CCH,0CCH : TH_96
DB 0CCC,0CCH,076H,000H,000H,000H : TH_96
DB 000H,060H,030H,018H,000H,0CCH,0CCH,0CCH : TH_97
DB 0CCC,0CCH,076H,000H,000H,000H : TH_97
DB 000H,000H,06CH,06CH,000H,06CH,0C6H,0C6H : TH_98
DB 0C6H,0C6H,07CH,000H,000H,000H : TH_98
DB 000H,000H,038H,06CH,0C6H,0C6H,0C6H,0C6H : TH_99
DB 0C6H,0C6H,038H,000H,000H,000H : TH_99
DB 000H,0C6H,0C6H,0C6H,0C6H,0C6H,0C6H,0C6H : TH_9A
DB 0C6H,0C6H,07CH,000H,000H,000H : TH_9A
DB 000H,018H,018H,018H,018H,018H,018H,018H : TH_9B
DB 0C6H,0C6H,07CH,000H,000H,000H : TH_9B
DB 000H,038H,06CH,06CH,000H,06CH,06CH,06CH : TH_9C
DB 060H,06EH,0FCH,000H,000H,000H : TH_9C
DB 000H,000H,066H,066H,03CH,018H,07EH,018H : TH_9D
DB 07EH,018H,018H,000H,000H,000H : TH_9D
DB 000H,0F8H,0CCH,0F8H,004H,0CCH,0DEH : TH_9E
DB 0CCH,0CCH,0C6H,000H,000H,000H : TH_9E
DB 000H,000H,018H,018H,018H,018H,018H,018H : TH_9F
DB 018H,018H,018H,018H,018H,018H,018H,018H : TH_9F
DB 000H,000H,060H,000H,078H,00CH,07CH : TH_A0
DB 0CCH,0CCH,076H,000H,000H,000H : TH_A0
DB 000H,018H,018H,030H,000H,038H,078H,018H : TH_A1
DB 018H,018H,030H,000H,000H,000H : TH_A1
DB 000H,018H,030H,000H,000H,000H,07CH,0C6H : TH_A2
DB 0C6H,0C6H,07CH,000H,000H,000H : TH_A2
DB 000H,018H,018H,000H,000H,000H,000H,000H : TH_A3
DB 0CCC,0CCH,076H,000H,000H,000H : TH_A3
DB 000H,000H,076H,00CH,000H,06CH,066H,06EH : TH_A4
DB 056H,066H,066H,000H,000H,000H : TH_A4
DB 075H,075H,075H,075H,075H,075H,075H,075H : TH_A5
DB 0CCH,0CCH,000H,000H,000H,000H : TH_A5
DB 000H,031H,06CH,06CH,031H,000H,07EH,000H : TH_A6
DB 000H,000H,000H,000H,000H,000H,000H,000H : TH_A6
DB 000H,038H,06CH,06CH,038H,000H,07CH,000H : TH_A7
DB 000H,000H,030H,030H,000H,010H,030H,060H : TH_A8
DB 0C6H,0C6H,07CH,000H,000H,000H : TH_A8
DB 000H,000H,000H,000H,000H,000H,000H,000H : TH_A9
DB 0C6H,0C6H,000H,000H,000H,000H,000H,000H : TH_AA
DB 000H,000H,000H,000H,000H,000H,000H,000H : TH_AA
DB 000H,000H,000H,000H,000H,000H,000H,000H : TH_AB
DB 0C6H,0C6H,000H,000H,000H,000H,000H,000H : TH_AB
DB 000H,000H,000H,000H,000H,000H,000H,000H : TH_AC
DB 0C6H,0C6H,000H,000H,000H,000H,000H,000H : TH_AC
DB 000H,000H,000H,000H,000H,000H,000H,000H : TH_AD
DB 03CH,03CH,018H,000H,000H,000H : TH_AD
DB 000H,000H,000H,000H,000H,000H,000H,000H : TH_AF
DB 036H,006H,000H,000H,000H,000H : TH_AF
DB 000H,000H,000H,000H,000H,000H,000H,000H : TH_AF
DB 0D8H,000H,000H,000H,000H,000H : BT_BF
DB 011H,04AH,011H,04AH,011H,04AH,011H,04AH : TH_B0
DB 011H,04AH,011H,04AH,011H,04AH,011H,04AH : TH_B0
DB 055H,0AAH,055H,0AAH,055H,0AAH,055H,0AAH : TH_B1
DB 055H,0AAH,055H,0AAH,055H,0AAH,055H,0AAH : TH_B1
DB 0DDH,077H,000H,077H,0DDH,077H,0DDH,077H : TH_B2
DB 018H,018H,018H,018H,018H,018H,018H,018H : TH_B3
DB 018H,018H,018H,018H,018H,018H,018H,018H : TH_B4
DB 018H,018H,018H,018H,018H,018H,018H,018H : TH_B4
DB 018H,018H,018H,018H,018H,018H,018H,018H : TH_B5
DB 018H,018H,018H,018H,018H,018H,018H,018H : TH_B5
DB 036H,036H,036H,036H,036H,036H,036H,036H : TH_B6
DB 000H,000H,000H,000H,000H,000H,000H,000H : TH_B7
DB 036H,036H,036H,036H,036H,036H,036H,036H : TH_B7
0A10 00 00 00 00 F8 570
0A18 18 18 18 18 18 571
0A1E 36 36 36 36 36 F6 572
0A24 36 36 36 36 36 573
0A26 36 36 36 36 36 574
0A2C 36 36 36 36 36 575
0A30 36 36 36 36 36 576
0A34 36 36 36 36 36 577
0A3A 00 00 00 00 FE 578
0A42 36 36 36 36 36 580
0A46 36 36 36 36 36 581
0A4A 36 36 36 36 36 582
0A50 00 00 00 00 00 583
0A56 36 36 36 36 36 584
0A5E 36 36 36 36 36 585
0A64 00 00 00 00 00 586
0A6A 18 18 18 18 18 F8 588
0A6C 18 F8 589
0A72 00 00 00 00 00 590
0A76 00 00 00 00 00 591
0A7A 18 18 18 18 18 592
0A80 18 18 18 18 18 594
0A86 18 18 18 18 18 595
0A8C 00 00 00 00 00 596
0A96 18 18 18 18 18 598
0AA0 00 00 00 00 00 600
0AA6 18 18 18 18 18 601
0AAE 18 18 18 18 18 602
0AB4 18 18 18 18 18 603
0AB8 18 18 18 18 18 604
0ABC 18 18 18 18 18 605
0ACB 18 18 18 18 18 606
0AD6 00 00 00 00 00 607
0AF0 00 00 00 00 00 609
0AC6 18 18 18 18 18 610
0ACE 18 18 18 18 18 612
0AD4 18 18 18 18 18 613
0ADC 18 18 18 18 18 614
0AE0 18 18 18 18 18 615
0AE6 18 18 18 18 18 616
0AF4 18 18 18 18 18 617
0AF8 36 36 36 36 36 618
0AFD 36 36 36 36 36 619
0B06 00 00 00 00 00 620
0B0E 00 00 00 00 00 621
0B16 36 36 36 36 36 623
0B1C 36 36 36 36 36 624
0B24 36 36 36 36 36 625
0B2A 00 00 00 00 00 627
0B30 00 00 00 00 00 628
0B36 36 36 36 36 36 630
0B3C 36 36 36 36 36 631
0B42 36 36 36 36 36 632
0B48 36 36 36 36 36 633
0B54 00 FF 00 00 00 634
0B5A 00 00 00 00 00 635
0B64 36 36 36 36 36 637
0B6A 00 00 00 00 00 638
0B76 18 18 18 18 18 640
0B7C 00 00 00 00 00 641
0B84 36 36 36 36 36 642
0B8A 36 36 36 36 36 643
0B96 36 36 36 36 36 644
0B9C 36 36 36 36 36 645
0BA2 00 00 00 00 00 646
0BA8 00 00 00 00 00 647
0BB4 18 18 18 18 18 648
0BB6 00 00 00 00 00 649
0BC2 00 00 00 00 00 650
0BC8 36 36 36 36 36 651
0BD4 36 36 36 36 36 652
0BDA 36 36 36 36 36 653
0BE6 36 36 36 36 36 654
0BE8 18 18 18 18 18 656
0BEC 18 18 18 18 18 657
0BF0 00 00 00 00 00 658
0BF6 00 00 00 00 00 659
0BF8 18 18 18 18 18 660
0BFC 18 18 18 18 18 661
0C04 36 36 36 36 36 662
0C0A 36 36 36 36 36 663
0C10 36 36 36 36 36 664
0C16 36 36 36 36 36 665
0C1C 36 36 36 36 36 666
0C22 36 36 36 36 36 667
0C28 36 36 36 36 36 668
0C2E 36 36 36 36 36 669
0C34 36 36 36 36 36 670
0C3A 36 36 36 36 36 671
0C40 18 18 18 18 18 672
0C46 18 18 18 18 18 673
0C4C 00 00 00 00 00 674
0C52 00 00 00 00 00 675
0C58 18 18 18 18 18 676
0C5E 18 18 18 18 18 677
0C64 00 00 00 00 00 678
0C6A 00 00 00 00 00 679
0C70 00 00 00 00 00 680
0C76 00 00 00 00 00 681
0C82 FF FF FF FF FF FF 682
0C88 FF FF FF FF FF FF 683
0C8E FF FF FF FF FF FF 684
0C94 FF FF FF FF FF FF 685
0C9A FF FF FF FF FF FF 686
0CA0 FF FF FF FF FF FF 687
0CA6 FF FF FF FF FF FF 688
0CAE FF FF FF FF FF FF 689
0CB4 FF FF FF FF FF FF 690
0CB8 00 00 00 00 OC 691
0CC0 DC 76 00 00 00 692
0CC4 DC 76 00 00 00 693
0CC8 DC 76 00 00 00 694
0CD0 DC 76 00 00 00 695

DB 000H,000H,000H,000H,0F8H,018H,0F8H ; TH_B8
DB 018H,018H,018H,018H,018H,018H,018H ; BT_B8
DB 036H,036H,036H,036H,036H,036H,0F6H ; TM_B9
DB 036H,036H,036H,036H,036H,036H,036H ; BT_B9
DB 036H,036H,036H,036H,036H,036H,036H ; TH_BA
DB 036H,036H,036H,036H,036H,036H,036H ; BT_BA
DB 000H,000H,000H,000H,000H,000H,0F6H ; TH_BB
DB 036H,036H,036H,036H,036H,036H,036H ; BT_BB
DB 036H,036H,036H,036H,036H,036H,0F6H ; TM_BC
DB 000H,000H,000H,000H,000H,000H,000H ; BT_BC
DB 036H,036H,036H,036H,036H,036H,036H ; TH_BC
DB 036H,036H,036H,036H,036H,036H,036H ; BT_BC
DB 000H,000H,000H,000H,000H,000H,0F6H ; TH_BE
DB 036H,036H,036H,036H,036H,036H,036H ; BT_BE
DB 036H,036H,036H,036H,036H,036H,0F6H ; TH_BF
DB 018H,018H,018H,018H,018H,018H,01FH ; TH_CO
DB 018H,018H,018H,018H,018H,018H,01FH ; BT_CO
DB 000H,000H,000H,000H,000H,000H,000H ; BT_CO
DB 036H,036H,036H,036H,036H,036H,036H ; TH_C1
DB 000H,000H,000H,000H,000H,000H,000H ; BT_C1
DB 018H,018H,018H,018H,018H,018H,01FH ; TH_C2
DB 018H,018H,018H,018H,018H,018H,01FH ; BT_C2
DB 018H,018H,018H,018H,018H,018H,01FH ; TH_C3
DB 018H,018H,018H,018H,018H,018H,01FH ; BT_C3
DB 000H,000H,000H,000H,000H,000H,000H ; BT_C4
DB 018H,018H,018H,018H,018H,018H,01FH ; TH_C5
DB 018H,018H,018H,018H,018H,018H,01FH ; BT_C5
DB 018H,018H,018H,018H,018H,018H,01FH ; TH_C6
DB 018H,018H,018H,018H,018H,018H,01FH ; BT_C6
DB 036H,036H,036H,036H,036H,036H,036H ; TH_CC
DB 036H,036H,036H,036H,036H,036H,036H ; BT_CC
DB 036H,036H,036H,036H,036H,036H,036H ; TH_CD
DB 036H,036H,036H,036H,036H,036H,036H ; BT_CD
DB 000H,000H,000H,000H,000H,000H,000H ; BT_CE
DB 036H,036H,036H,036H,036H,036H,036H ; TH_CF
DB 036H,036H,036H,036H,036H,036H,036H ; BT_CF
DB 000H,000H,000H,000H,000H,000H,000H ; BT_CF
DB 036H,036H,036H,036H,036H,036H,036H ; TH_D0
DB 000H,000H,000H,000H,000H,000H,000H ; BT_D0
DB 018H,018H,018H,018H,018H,018H,01FH ; TH_D1
DB 018H,018H,018H,018H,018H,018H,01FH ; BT_D1
DB 036H,036H,036H,036H,036H,036H,036H ; TH_D2
DB 036H,036H,036H,036H,036H,036H,036H ; BT_D2
DB 000H,000H,000H,000H,000H,000H,000H ; BT_D3
DB 018H,018H,018H,018H,018H,018H,01FH ; TH_D4
DB 000H,000H,000H,000H,000H,000H,000H ; BT_D4
DB 018H,018H,018H,018H,018H,018H,01FH ; TH_D5
DB 018H,018H,018H,018H,018H,018H,01FH ; BT_D5
DB 000H,000H,000H,000H,000H,000H,000H ; BT_D6
DB 036H,036H,036H,036H,036H,036H,036H ; TH_D7
DB 036H,036H,036H,036H,036H,036H,036H ; BT_D7
DB 000H,000H,000H,000H,000H,000H,000H ; BT_D8
DB 018H,018H,018H,018H,018H,018H,01FH ; TH_D9
DB 000H,000H,000H,000H,000H,000H,000H ; BT_D9
DB 018H,018H,018H,018H,018H,018H,01FH ; TH_DA
DB 018H,018H,018H,018H,018H,018H,01FH ; BT_DA
DB 0FFH,0FFH,0FFH,0FFH,0FFH,0FFH,0FFH ; TH_DB
DB 0FFH,0FFH,0FFH,0FFH,0FFH,0FFH,0FFH ; BT_DB
DB 0FFH,0FFH,0FFH,0FFH,0FFH,0FFH,0FFH ; TH_DC
DB 0FFH,0FFH,0FFH,0FFH,0FFH,0FFH,0FFH ; BT_DC
DB 0FFH,0FFH,0FFH,0FFH,0FFH,0FFH,0FFH ; TH_DD
DB 0FFH,0FFH,0FFH,0FFH,0FFH,0FFH,0FFH ; BT_DD
DB 0FFH,0FFH,0FFH,0FFH,0FFH,0FFH,0FFH ; TH_DE
DB 0FFH,0FFH,0FFH,0FFH,0FFH,0FFH,0FFH ; BT_DE
DB 000H,000H,000H,000H,000H,000H,000H ; BT_DF
DB 000H,000H,000H,000H,000H,000H,000H ; TH_E0
DB 000H,000H,000H,000H,000H,000H,000H ; BT_E0
DB 000H,000H,000H,000H,000H,000H,000H ; TH_E1
DB 000H,000H,000H,000H,000H,000H,000H ; BT_E1
DC4E 03 00 00 7C C6 696
FC FE 697
DC56 FC FC CO 00 00 698
DC5C 00 FE C6 C6 00 699
DC5A CO CO 00 00 00 701
DC6A 00 FE FE 6C 702
DC6B 6C 00 00 00 703
DC72 6C 6C 00 00 704
DC78 18 30 00 00 705
DC80 60 00 1F FE 707
DC81 00 00 00 00 708
DC88 DB 00 00 00 709
DC8E 00 00 00 00 66 710
DC94 66 66 00 00 711
DC9C 00 00 00 76 DC 713
DCAA 18 18 00 00 715
DCB0 00 7E 18 3C 6E 717
DCBB 18 7E 00 00 719
DCBE 00 38 6C C6 720
DCC6 6C 38 00 00 722
DCC7 6C 38 00 00 723
DCC9 6C 6C 00 00 725
DCCA 6C 6C 00 00 726
DCCF 3E 3E 00 00 727
DCE2 00 00 00 00 729
DCE5 00 00 00 00 730
DCF0 00 00 00 00 731
DCF6 00 03 06 7E DB 732
DCF7 F3 733
DCF8 00 00 00 00 734
DD04 00 1C 30 60 735
DD0C 30 1C 00 00 736
DD12 00 00 00 7C C6 738
DD1A C6 C6 00 00 740
DD20 00 00 FE 00 742
DD28 FE FE 743
DD2E 00 00 18 18 745
DD36 00 FF 00 00 747
DD3C 00 30 18 OC 06 748
DD44 30 7E 00 00 750
DD4A 00 18 30 60 751
DD52 00 00 00 00 753
DD54 00 00 00 00 754
DD60 18 18 18 18 18 756
DD68 18 18 18 18 758
DD74 00 00 00 00 759
DD7A 00 00 18 18 760
DD7C 18 00 00 00 761
DD82 00 00 00 76 DC 763
DD8A 00 00 00 00 765
DD90 38 6C 38 00 766
DD98 00 00 00 00 767
DD9E 00 00 00 00 769
DDAC 00 00 00 00 771
DDA6 00 00 00 00 772
DB04 00 00 00 00 774
DB0A 00 00 00 00 775
DB1C 00 1C 00 00 776
DB28 6C 6C 00 00 778
DB30 00 00 00 00 779
DB36 00 00 00 00 780
DB3D 00 00 00 00 781
DB4E 00 00 00 00 782
DB5E 00 00 00 00 783
DB64 00 00 00 00 784
DB6C 7C 7C 00 00 786
DB7F 00 00 00 00 787
DB8A 00 00 00 00 788
DB90 00 00 00 00 789
DB96 79 79 790
CODE ENDS

PAGE_120
SUBTITLE MONOCHROME CHARACTER GENERATOR - ALPHA SUPPLEMENT
SEGMENT PUBLIC
PUBLIC COMM_FDG
LABEL BYTE

STRUCTURE OF THIS FILE
WHERE XX IS THE HEX CODE FOR THE FOLLOWING CHAR
BYTES 0 - 13 OF THAT CHARACTER
DB 00H INDICATES NO MORE REPLACEMENTS TO BE DONE

0000 10 74
0001 00 00 24 66 15
0009 24 00 00 00 00 16
0010 63 63 63 22 19
0018 00 00 00 00 00 20
001B 00 00 18 18 22
001F 18 18 00 00 25
0022 18 18 00 00 26
002E 00 00 00 00 28
0000,0000,0000,0000,07CH,0C6H,0FCH,0C6H ; TH_E1
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_E1
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_E2
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_E2
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_E3
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_E3
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_E4
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_E4
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_E5
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_E5
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_E6
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_E6
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_E7
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_E7
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_E8
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_E8
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_E9
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_E9
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_EA
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_EA
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_EB
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_EB
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_EC
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_EC
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_ED
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_ED
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_EE
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_EE
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_EF
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_EF
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_F0
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_F0
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_F1
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_F1
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_F2
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_F2
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_F3
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_F3
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_F4
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_F4
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_F5
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_F5
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_F6
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_F6
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_F7
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_F7
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_F8
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_F8
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_F9
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_F9
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_FA
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_FA
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_FB
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_FB
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_FC
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_FC
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_FD
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_FD
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_FE
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_FE
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; TH_FF
0000,0000,0000,0000,06FH,0C6H,0C6H,0C6H ; BT_FF
0036 00 00 00 00 00 29
003C 4D 00 00 C3 E7 FF DB
003D C3 C3 00 00 00 00 31
0045 C3 C3 00 00 00 00 33
004A 00 00 DB 9B 18 18 36
004C 00 00 DB 9B 18 18 35
0054 00 00 C3 3C 00 00 00 37
005A 56 00 00 C3 C3 C3 38
005B 00 00 C3 C3 C3 C3 39
0063 66 3C 18 00 00 00 40
0064 00 00 C3 C3 C3 42
006A 00 00 C3 C3 C3 43
006E 66 66 00 00 00 45
0072 66 66 00 00 00 46
0078 58 00 00 C3 66 3C 47
0081 66 C3 C3 00 00 00 49
0087 66 C3 C3 00 00 00 50
0088 00 00 C3 C3 66 3C 51
0090 18 18 3C 00 00 00 53
0094 5A 00 00 FF C3 86 OC 55
009F 18 30 00 00 00 56
00A5 E9 00 00 00 00 E6 59
00A6 00 00 00 00 00 E6 59
00AE 00 00 DB 00 00 00 61
00B5 76 00 00 00 00 C3 63
00B7 66 3C 18 00 00 00 65
00CC 77 00 00 00 00 C3 67
00CD 00 00 00 00 00 C3 68
00D3 00 00 00 00 6E 3B 71
00D9 00 00 00 77 00 00 73
00E1 98 00 18 18 7E C3 75
00E2 75 00 18 18 7E C3 75
00EA 7E 18 18 00 00 00 76
00F1 00 00 C3 66 3C 18 79
00F9 18 FE 18 00 00 00 80
00FE 9E 00 18 18 00 00 81
0100 FC 66 66 7C 62 83
0108 66 F3 00 00 00 85
010F 00 00 18 18 18 FF 87
0117 FF 00 00 00 00 00 89
011D F6 00 18 18 60 00 90
011E 00 18 18 00 00 00 91
0126 00 18 18 00 00 00 93
012D 95 CODE 95
ENDS

PAGE 120
SUBTTL DOUBLE DOT CHARACTER GENERATOR
CODE SEGMENT PUBLIC
PUBLIC CGDDOT, INT_TF_1
LABEL BYTE
CGDDOT
0000 00 00 00 00 00 00 00 00 ;DOUBLE DOT
0008 01 7E A5 81 BD 99 09 ;D_00
0010 81 7E 00 00 DB F3 11 ;D_01
0018 FE FE FE FE 7C 38 13 ;D_02
0020 1C 38 FE FE FE 7C 38 15 ;D_03
0028 38 7C 38 FE FE FE 7C 17 ;D_04
0030 38 7C 38 7C 38 FE FE 19 ;D_05
0038 00 18 18 3C 18 20 ;D_06
0040 FF FE E7 C3 C3 E7 22 ;D_07
0048 00 3C 66 42 42 42 24 ;D_08
0050 99 99 BD 80 99 26 ;D_09
0058 0F 0F 07 70 CC CC 28 ;D_10
0060 3C 66 66 66 3C 18 31 ;D_11
0068 33 3F 33 30 30 30 33 ;D_12
0070 0F 0F 07 FE 63 63 67 35 ;D_13
0078 66 C0 00 00 00 00 36 ;D_14
0080 99 99 99 99 99 99 38 ;D_15
0088 00 E0 FE FE FE E0 40 ;D_16
0090 02 0E 3E FE 3E 0E 42 ;D_17
0098 18 3C 18 18 18 18 44 ;D_18
009E 66 66 66 66 66 66 46 ;D_19
00A0 00 DB 7B 1B 1B 48 ;D_20
00A8 00 3C 7A 3C 6C 38 50 ;D_21
00B0 00 00 00 00 00 7E 52 ;D_22
00B8 18 3C 18 18 7E 3C 54 ;D_23
00C0 18 3C 18 18 18 18 56 ;D_24
0000 00 00 00 00 00 00 00 00 ;BT_20 -
0040 04DH 000H,000H,000H,000H,000H ;TH_4D M
0048 000H,000H,0C3H,0E7H,0FFH,0DBH,0C3H,0C3H ;TH_4D M
0050 05FH,000H,000H,000H,000H,000H ;BT_54 T
0058 018H,018H,03CH,000H,000H,000H ;BT_54 T
005E 056H 000H,000H,0C3H,0C3H,0C3H,0C3H,0C3H,0C3H ;TH_56 V
0060 06FH,03CH,018H,000H,000H,000H ;BT_56 V
0062 05FH,000H,000H,0C3H,0C3H,0C3H,0C3H,0DBH ;TH_57 W
0068 0FFH,066H,066H,000H,000H,000H ;BT_57 W
006E 058H 000H,000H,0C3H,0C3H,066H,03CH,018H,03CH ;TH_58 X
0070 066H,0C3H,0C3H,000H,000H,000H ;BT_58 X
0072 059H 000H,000H,0C3H,0C3H,0C3H,066H,03CH,018H ;TH_59 Y
0074 018H,018H,03CH,000H,000H,000H ;BT_59 Y
0076 05AH 000H,000H,0FFH,0C3H,086H,00CH,018H,030H ;TH_5A Z
0078 061H,0C3H,0FFH,000H,000H,000H ;BT_5A Z
007A 060H,000H,000H,000H,000H,000H,000H,000H ;TH_60 L.C. M
007C 0DBH,0DBH,0DBH,000H,000H,000H ;BT_60 L.C. M
007E 076H 000H,000H,000H,000H,0C3H,0C3H,0C3H,0C3H ;TH_76 L.C. V
0080 066H,03CH,018H,000H,000H,000H ;BT_76 L.C. V
0082 077H 000H,000H,000H,000H,0C3H,0C3H,0DBH ;TH_77 L.C. W
0084 09TH 000H,000H,000H,000H,000H,000H ;BT_77 L.C. W
0086 0DBH,0FFH,066H,000H,000H,000H ;BT_77 L.C. W
0088 07BH,000H,000H,000H,000H,000H,000H,000H ;TH_91
008A 09BH 000H,018H,018H,07EH,000H,000H,000H,000H ;TH_98
008C 07EH,018H,018H,000H,000H,000H ;BT_98
008E 09DH 000H,000H,0C3H,066H,03CH,018H,0FFH,01AH ;TH_9D
0090 0FTH,018H,018H,000H,000H,000H ;BT_9D
0092 09EN 000H,0FFH,066H,066H,07CH,066H,066H,06FH ;TH_9E
0094 066H,066H,0F3H,000H,000H,000H ;BT_9E
0096 0FTH,018H,018H,018H,018H,0FFH,018H,018H ;TH_F1
0098 018H,000H,0FFH,000H,000H,000H ;BT_F1
009A 0F6H 000H,000H,018H,018H,000H,000H,0FFH,000H ;TH_F6
009C 000H,018H,018H,000H,000H,000H ;BT_F6
009E NO MORE
00C8 18 18 18 7E 3C 57 DB 018H,018H,018H,018H,07EH,03CH,018H,000H ; D_19
0000 18 0C FE OC 18 DB 000H,018H,00CH,3FEH,00CH,018H,000H,000H ; D_1A
0008 20 60 FE 60 30 62 DB 000H,030H,060H,0FEH,060H,030H,000H,000H ; D_1B
00E0 00 00 CO CO FE 64 DB 000H,000H,000H,000H,000H,000H,000H,000H ; D_1C
00E5 00 00 66 FF 66 24 DB 000H,024H,066H,0FFH,066H,024H,000H,000H ; D_1D
00F0 18 3C 7E FF FF 68 DB 000H,018H,03CH,07EH,0FFH,0FFH,000H,000H ; D_1E
00F8 00 FF FF 7E 3C 18 DB 000H,0FFH,0FFH,07EH,03CH,018H,000H,000H ; D_1F
0100 00 00 00 00 00 77 DB 000H,000H,000H,000H,000H,000H,000H,000H ; SP_D_20
0108 30 78 78 30 30 75 DB 030H,078H,078H,030H,030H,000H,030H,000H ; ! D_21
0110 6C 6C 6C 00 00 76 DB 06CH,06CH,06CH,000H,000H,000H,000H ; # D_22
0118 00 00 FE FC FE 6C DB 06CH,06CH,0FEH,06CH,06CH,06CH,000H ; # D_23
0120 7C CC 78 OC F8 81 DB 030H,07CH,0C0H,078H,000H,078H,030H,000H ; S_O_24
0128 0C CC 18 30 66 84 DB 000H,006H,0CCH,018H,030H,066H,000H,000H ; PER CENT_D_25
0130 38 6C 38 76 DC CC 85 DB 038H,06CH,038H,076H,0DCH,0CCH,076H,000H ; & D_26
0138 0C 00 00 00 00 87 DB 060H,000H,000H,000H,000H,000H,000H ; ! D_27
0140 60 60 60 60 30 69 DB 018H,030H,060H,060H,030H,018H,000H ; ! D_28
0148 60 30 18 18 30 91 DB 060H,030H,018H,018H,030H,060H,000H ; ! D_29
0150 66 3C FF 3C 66 93 DB 000H,066H,03CH,0FFH,03CH,066H,000H ; * D_2A
0158 00 30 FC 30 30 94 DB 000H,030H,030H,0FCH,030H,030H,000H ; + D_2B
0160 00 00 00 00 00 97 DB 000H,000H,000H,000H,000H,000H,000H ; . D_2C
0168 00 00 00 FC 00 99 DB 000H,000H,000H,0FCH,000H,000H,000H ; - D_2D
0170 00 00 00 00 00 101 DB 000H,000H,000H,000H,000H,000H,000H ; . D_2E
0178 0C 18 30 60 CO 103 DB 006H,00CH,018H,030H,060H,000H,000H ; / D_2F
0180 7C C6 CE DE FE 106 DB 07CH,0C6H,0CEH,0DEH,0FEH,07CH,000H ; 0_D_30
0188 30 70 30 30 30 108 DB 030H,070H,030H,030H,030H,030H,000H ; 1_D_31
0190 0C 00 0C 38 60 CC 110 DB 678H,00CH,00CH,038H,060H,0CCH,0FCH,000H ; 2_D_32
0198 0C 00 0C 38 OC CC 112 DB 078H,00CH,00CH,038H,00CH,0CCH,078H,000H ; 3_D_33
01A0 1C 3C 6C CC FE OC 114 DB 01CH,03CH,06CH,0CCH,0FEH,00CH,01EH,000H ; 4_D_34
01A8 FC 08 FC OC CC 119 DB 0FCH,00CH,0F8H,00CH,0CCH,0CCH,078H,000H ; 5_D_35
01B0 78 00 FC 08 CC 117 DB 038H,000H,0F8H,00CH,0CCH,0CCH,078H,000H ; 6_D_36
01B8 78 CC OC 18 30 120 DB 078H,00CH,00CH,018H,030H,030H,000H ; 7_D_37
01C0 78 CC 7C 78 CC 122 DB 078H,00CH,00CH,078H,00CH,0CCH,078H,000H ; 8_D_38
01C8 78 CC 7C OC 18 124 DB 078H,00CH,00CH,078H,00CH,018H,070H,000H ; 9_D_39
01D0 00 30 00 00 30 126 DB 000H,030H,030H,000H,000H,030H,030H,000H ; 10_D_3A
01D8 30 00 30 00 30 127 DB 000H,030H,030H,000H,000H,030H,030H,000H ; 11_D_3B
01EC 18 30 60 CO 60 131 DB 018H,030H,060H,000H,030H,018H,000H ; < D_3C
01F0 00 FC 00 OC FC 132 DB 000H,000H,0FCH,000H,000H,000H,000H ; > D_3D
01F8 00 30 18 OC 18 134 DB 060H,030H,018H,00CH,018H,030H,060H,000H ; > D_3E
0200 0C 00 0C 18 30 136 DB 078H,00CH,00CH,018H,030H,000H,030H,000H ; ? D_3r
0208 7C C6 DE DE CO 140 DB 07CH,0C6H,0DEH,0DEH,0DEH,0COH,078H,000H ; @ D_40
020C 78 00 CC CC FC CC 141 DB 030H,078H,000H,0CCH,0CCH,0CCH,0CCH,000H ; A_D_41
0210 66 66 7C 66 66 143 DB 06CH,066H,066H,07CH,066H,066H,06CH,000H ; B_D_42
0218 3C 66 CO CO 66 145 DB 03CH,066H,0CCOH,0CCOH,066H,03CH,000H ; C_D_43
0220 86 66 66 66 66 146 DB 0F8H,066H,066H,066H,066H,066H,06FH,000H ; D_D_44
0228 FE 68 78 68 68 149 DB 0FEH,062H,068H,078H,068H,062H,0FEH,000H ; E_D_45
0230 FE 68 78 68 60 151 DB 0FEH,062H,068H,078H,068H,060H,0FEH,000H ; F_D_46
0238 3C 66 CO CO 66 153 DB 03CH,066H,0CCOH,0CCOH,066H,03EH,000H ; G_D_47
0240 0C 00 0C 00 0C 154 DB 00CH,00CH,00CH,00CH,00CH,00CH,00CH,000H ; H_D_48
0248 30 30 30 30 30 157 DB 078H,030H,030H,030H,030H,030H,078H,000H ; I_D_49
0250 1C 00 OC CC CC 159 DB 01EH,000H,00CH,0CCH,0CCH,078H,000H ; J_D_4A
0258 66 66 66 7C 66 161 DB 06EH,066H,066H,078H,066H,066H,06EH,000H ; K_D_4B
0260 00 60 60 62 66 163 DB 0F0H,060H,060H,062H,066H,06FH,000H ; L_D_4C
0268 FE FE FE FE D6 C6 164 DB 0FEH,062H,068H,078H,068H,062H,0FEH,000H ; M_D_4D
0270 C6 00 66 DE DE C6 166 DB 03CH,066H,000H,066H,0DEH,0DEH,000H ; N_D_4E
0278 3C 66 CC C6 C6 169 DB 03CH,066H,0CCOH,0C6H,0C6H,03EH,000H ; O_D_4F
0280 FC 66 66 7C 60 60 172 DB 0FCH,066H,066H,07CH,060H,060H,0F0H,000H ; P_D_50
0288 78 CC OC CC DC 174 DB 078H,00CH,00CH,0CCH,0CCH,078H,01CH,000H ; Q_D_51
0290 FE 66 66 7C 66 176 DB 0F8H,066H,066H,07CH,066H,066H,06FH,000H ; R_D_52
0298 FE E0 70 1C CC 178 DB 078H,00CH,0E0H,070H,01CH,0CCH,078H,000H ; S_D_53
02A0 78 00 30 30 30 180 DB 078H,000H,030H,030H,030H,030H,078H,000H ; T_D_54
02A8 CC CC CC CC CC 182 DB 0CCCH,0CCCH,0CCCH,0CCCH,0CCCH,0CCCH,0FCCH,000H ; U_D_55
0280 FC 00 183
CC 00 CC CC 78 184
0282 C6 00 185
C6 D6 FE EE 186
0284 C6 00 187
C6 38 38 6C 188
0286 C6 00 189
CC 78 30 30 190
78 00 191
0289 C6 8C 18 32 66 192
FE 00 193
028D 60 60 60 60 194
028F C0 00 195
C0 30 18 OC 06 196
0291 00 197
18 18 18 18 18 198
78 00 199
0295 18 6C C6 00 00 200
00 00 201
FF 00 202
0298 00 FF 203
0300 30 30 18 00 00 205
00 00 206
0308 00 7E 0C 7C 0C 207
0310 60 60 7C 66 66 208
DC 00 209
0318 78 78 CC CC 210
031A 00 211
031C 0C 0C 7C 7C 212
031E 00 213
0320 78 78 CC FC 215
00 00 216
38 6C 60 60 60 217
00 00 218
0338 76 7C 7C 7C 219
0C F8 220
0340 6C 76 66 66 221
0342 00 222
30 70 30 30 30 223
0346 00 224
00 OC OC OC OC 225
CC 78 226
0358 66 6C 78 6C 227
E6 00 228
78 30 30 30 30 229
0368 00 CC FE FE 06 231
C6 00 232
0370 00 F8 CC CC CC 233
0378 78 CC CC CC 235
0380 00 DC 66 6C 7C 237
60 60 239
0388 76 CC CC CC 240
0C 1E 241
0390 DC 76 66 60 242
79 00 243
0398 00 CC 7C 78 7C 244
03A0 10 10 30 30 34 245
03A8 00 CC CC CC CC 246
76 00 248
03B0 00 CC CC CC 78 249
30 00 250
03B8 00 C6 D6 FE FE 252
C6 38 253
03C0 00 CC 6C 38 6C 254
03C8 00 CC CC CC 7C 256
03D0 FC 98 30 64 257
7C 00 258
03D8 30 30 E0 30 30 260
1C 00 261
03E0 18 18 18 18 18 262
03E8 E0 30 1C 30 30 263
03F0 00 00 00 00 00 265
03F8 00 00 266
18 38 6C C6 C6 268
FE 00 269
0400 78 CC CC 78 18 271
0C 78 272
0408 00 CC CC CC CC 275
0410 1C 00 277
0418 1C 06 3E 66 278
0420 78 0C 7C CC 279
0E 00 281
0428 78 0C 7C CC 283
7E 00 284
0430 30 78 0C 7C CC 286
0438 78 CC CO 78 287
0C 38 288
0440 3C 66 7E 6C 289
3C 00 290
0448 78 CC FC CC 291
0E 00 292
0450 78 CC FC CO 293
7E 00 294
0458 00 70 30 30 30 295
78 00 296
0460 38 18 18 18 18 297
0468 70 30 30 30 30 300
0470 38 6C C6 FE C6 301
CC 00 302
0478 30 00 78 CC FC 303
CC 00 304
0480 1C 00 FC 60 78 50 305
0488 00 7F OC 7F CC 306

DB OCCH, OCCH, OCCH, OCCH, OCCH, 07BH, 030H, 000H ; V_D_56
DB OC6H, OC6H, OC6H, OC6H, OFEH, OEEN, OC6H, 000H ; W_D_57
DB OC6H, OC6H, OC6H, 038H, 038H, OC6H, OC6H, 000H ; X_D_58
DB OCCH, OCCH, OCCH, 07BH, 030H, 030H, 07BH, 000H ; Y_D_59
DB OFEH, OC6H, OC6H, 018H, 032H, 066H, OFEH, 000H ; Z_D_5A
DB 07BH, 060H, 060H, 060H, 060H, 060H, 07BH, 000H ; [ D_5B
DB OC0H, OC0H, (010H, 018H, OC0H, 006H, 002H, 000H ; \BACKSLASH_D_5C
DB 078H, 018H, 018H, 018H, 018H, 018H, 078H, 000H ; ]_D_50
DB 010H, 038H, OC6H, OC6H, 000H, 000H, 000H, 000H ; _CIRCUMFLEX_D_5E
DB 000H, 000H, 000H, 000H, 000H, 000H, 000H, OFMH ; _D_5F
DB 030H, 030H, 018H, 000H, 000H, 000H, 000H, 000H ; 'D_60
DB 000H, 000H, 07BH, OCCH, 07CH, OCCH, 076H, 000H ; LOWER CASE A_D_61
DB 0E0H, 060H, 060H, 07CH, 066H, 066H, OCCH, 000H ; L.C. B_D_62
DB 000H, 000H, 07BH, OCCH, OCCH, 078H, 000H ; L.C. C_D_63
DB 01CH, OCCH, 06CH, 07CH, OCCH, OCCH, 076H, 000H ; L.C. D_D_64
DB 000H, 000H, 07BH, OCCH, OCCH, OCCH, 078H, 000H ; L.C. E_D_65
DB 038H, 06CH, 060H, 0F0H, 060H, 0F0H, 000H, L.C. F_D_66
DB 000H, 000H, 076H, OCCH, 07CH, OCCH, 0F8H ; L.C. G_D_67
DB 0E0H, 060H, 060H, 076H, 066H, 066H, OCCH, 000H ; L.C. H_D_68
DB 030H, 000H, 070H, 030H, 030H, 030H, 07BH, 000H ; L.C. I_D_69
DB 000H, 000H, 07BH, OCCH, OCCH, OCCH, OCCH, 078H ; L.C. J_D_6A
DB 0E0H, 060H, 064H, 06CH, 078H, 06CH, 060H, 000H ; L.C. K_D_6B
DB 070H, 030H, 030H, 030H, 030H, 030H, 07BH, 000H ; L.C. L_D_6C
DB 000H, 000H, OCCH, OCCH, OCCH, OCCH, OCCH, OCCH ; L.C. M_D_6D
DB 000H, 000H, 07BH, OCCH, OCCH, OCCH, OCCH, OCCH ; L.C. N_D_6E
DB 000H, 000H, 07BH, OCCH, OCCH, OCCH, OCCH, OCCH ; L.C. O_D_6F
DB 000H, 000H, OCCH, 066H, 07CH, 060H, 0F0H ; L.C. P_D_70
DB 000H, 000H, 076H, OCCH, OCCH, OCCH, OCCH, OCCH ; L.C. Q_D_71
DB 000H, 000H, OCCH, 076H, 064H, 060H, 0F0H, 000H ; L.C. R_D_72
DB 000H, 000H, 07CH, OCCH, 07BH, OCCH, 0F8H, 000H ; L.C. S_D_73
DB 010H, 030H, 07CH, 030H, 030H, 034H, 018H, 000H ; L.C. T_D_74
DB 000H, 000H, OCCH, OCCH, OCCH, OCCH, 076H, 000H ; L.C. U_D_75
DB 000H, 000H, OCCH, OCCH, OCCH, OCCH, 078H, 000H ; L.C. V_D_76
DB 000H, 000H, OCCH, OCCH, OCCH, OCCH, OCCH, OCCH ; L.C. W_D_77
DB 000H, 000H, OCCH, OCCH, OCCH, OCCH, OCCH, OCCH ; L.C. X_D_78
DB 000H, 000H, OCCH, OCCH, OCCH, OCCH, OCCH, OCCH ; L.C. Y_D_79
DB 000H, 000H, OCCH, 098H, 030H, 064H, 0FCH, 000H ; L.C. Z_D_7A
DB 01CH, 030H, 030H, 030H, 030H, 030H, 018H, 000H ; L.BRAK_D_7B
DB 018H, 018H, 018H, 018H, 018H, 018H, 018H, 000H ; I_D_7C
DB 0E0H, 030H, 030H, 01CH, 030H, 030H, 0E0H, 000H ; R_BRAK_D_7D
DB 076H, OCCH, 000H, 000H, 000H, 000H, 000H, 000H ; TILDE_D_7E
DB 000H, 010H, 038H, OC6H, OC6H, OFEH, 000H ; DELTA_D_7F

INT_1F_1 LABEL BYTE
078H, OCCH, OCCH, OCCH, 07BH, 018H, OCCH, 078H ; D_80
000H, OCCH, 000H, OCCH, OCCH, OCCH, 07EH, 000H ; _B_81
01CH, 07BH, OCCH, OCCH, OCCH, OCCH, 078H, 000H ; D_82
07EH, OC3H, 033H, 063H, 03EH, 063H, 03FH, 000H ; D_83
OCCH, 000H, 078H, OCCH, 07CH, OCCH, 07EH, 000H ; D_84
0E0H, 000H, 078H, 00CH, 07CH, OCCH, OCCH, 07EH ; D_85
030H, 030H, 078H, 06CH, 07CH, OCCH, OCCH, 000H ; D_86
000H, 000H, 078H, OCCH, OCCH, 078H, OCCH, 038H ; D_87
07EH, OC3H, 033H, 066H, 07EH, 060H, 03CH, 000H ; D_88
OCCH, 000H, 078H, OCCH, OCCH, OCCH, 078H, 000H ; D_89
0E0H, 000H, 078H, OCCH, OCCH, OCCH, 078H, 000H ; D_8A
OCCH, 000H, 070H, 030H, 030H, 030H, 078H, 000H ; D_8B
07CH, OC6H, 038H, 018H, 018H, 018H, 03CH, 000H ; D_8C
0E0H, 000H, 070H, 030H, 030H, 030H, 078H, 000H ; D_8D
OC6H, 038H, 06CH, OFEH, OC6H, OC6H, 000H ; D_8E
030H, 030H, 000H, 078H, OCCH, OCCH, OCCH, 000H ; D_8F
01CH, 000H, 0FCH, 060H, 078H, 060H, 0FCH, 000H ; D_90
000H, 000H, 07FH, OCCH, 07FH, OCCH, 07FH, 000H ; D_91
0490 7F 00 FE CC CC 309
0498 7E 00 78 CC CC 311
04A0 78 00 78 CC CC 312
04A8 78 00 78 CC CC 313
04B0 78 00 78 CC CC 314
04B8 78 00 78 CC CC 315
04C0 78 00 78 CC CC 316
04C8 78 00 78 CC CC 317
04D0 78 00 78 CC CC 318
04D8 78 00 78 CC CC 319
04E0 78 00 78 CC CC 320
04E8 78 00 78 CC CC 321
04F0 78 00 78 CC CC 322
04F8 78 00 78 CC CC 323
0500 78 00 78 CC CC 324
0508 78 00 78 CC CC 325
0510 78 00 78 CC CC 326
0518 78 00 78 CC CC 327
0520 78 00 78 CC CC 328
0528 78 00 78 CC CC 329
0530 78 00 78 CC CC 330
0538 78 00 78 CC CC 331
0540 78 00 78 CC CC 332
0548 78 00 78 CC CC 333
0550 78 00 78 CC CC 334
0558 78 00 78 CC CC 335
0560 78 00 78 CC CC 336
0568 78 00 78 CC CC 337
0576 78 00 78 CC CC 338
0580 78 00 78 CC CC 339
0588 78 00 78 CC CC 340
0590 78 00 78 CC CC 341
0598 78 00 78 CC CC 342
05A0 78 00 78 CC CC 343
05A8 78 00 78 CC CC 344
05B0 78 00 78 CC CC 345
05B8 78 00 78 CC CC 346
05C0 78 00 78 CC CC 347
05C8 78 00 78 CC CC 348
05D0 78 00 78 CC CC 349
05D8 78 00 78 CC CC 350
05E0 78 00 78 CC CC 351
05E8 78 00 78 CC CC 352
05F0 78 00 78 CC CC 353
05F8 78 00 78 CC CC 354
0600 78 00 78 CC CC 355
0608 78 00 78 CC CC 356
0610 78 00 78 CC CC 357
0618 78 00 78 CC CC 358
0620 78 00 78 CC CC 359
0628 78 00 78 CC CC 360
0630 78 00 78 CC CC 361
0638 78 00 78 CC CC 362
0640 78 00 78 CC CC 363
0648 78 00 78 CC CC 364
0650 78 00 78 CC CC 365
0658 78 00 78 CC CC 366
0660 78 00 78 CC CC 367
0668 78 00 78 CC CC 368
0670 78 00 78 CC CC 369
0678 78 00 78 CC CC 370
0680 78 00 78 CC CC 371
0688 78 00 78 CC CC 372
0690 78 00 78 CC CC 373
0698 78 00 78 CC CC 374
06A0 78 00 78 CC CC 375
06A8 78 00 78 CC CC 376
06B0 78 00 78 CC CC 377
06B8 78 00 78 CC CC 378
06C0 78 00 78 CC CC 379
06C8 78 00 78 CC CC 380
06D0 78 00 78 CC CC 381
06D8 78 00 78 CC CC 382
06E0 78 00 78 CC CC 383
06E8 78 00 78 CC CC 384
06F0 78 00 78 CC CC 385
06F8 78 00 78 CC CC 386
0700 78 00 78 CC CC 387
0708 78 00 78 CC CC 388
0710 78 00 78 CC CC 389
0718 78 00 78 CC CC 390
0720 78 00 78 CC CC 391
0728 78 00 78 CC CC 392
0730 78 00 78 CC CC 393
0738 78 00 78 CC CC 394
0740 78 00 78 CC CC 395
0748 78 00 78 CC CC 396
0750 78 00 78 CC CC 397
0758 78 00 78 CC CC 398
0760 78 00 78 CC CC 399
0768 78 00 78 CC CC 400
0770 78 00 78 CC CC 401
0778 78 00 78 CC CC 402
0780 78 00 78 CC CC 403
0788 78 00 78 CC CC 404
0790 78 00 78 CC CC 405
0798 78 00 78 CC CC 406
07A0 78 00 78 CC CC 407
07A8 78 00 78 CC CC 408
07B0 78 00 78 CC CC 409
07B8 78 00 78 CC CC 410
07C0 78 00 78 CC CC 411
07C8 78 00 78 CC CC 412
07D0 78 00 78 CC CC 413
07D8 78 00 78 CC CC 414
07E0 78 00 78 CC CC 415
07E8 78 00 78 CC CC 416
07F0 78 00 78 CC CC 417
07F8 78 00 78 CC CC 418
0800 78 00 78 CC CC 419
0808 78 00 78 CC CC 420
0810 78 00 78 CC CC 421
0818 78 00 78 CC CC 422
0820 78 00 78 CC CC 423
0828 78 00 78 CC CC 424
0830 78 00 78 CC CC 425
0838 78 00 78 CC CC 426
0840 78 00 78 CC CC 427
0848 78 00 78 CC CC 428
0850 78 00 78 CC CC 429
0858 78 00 78 CC CC 430
0860 78 00 78 CC CC 431
0868 78 00 78 CC CC 432
0870 78 00 78 CC CC 433
0878 78 00 78 CC CC 434
DB 03EH, 06CH, OCCH, OFEH, OCCH, OCCH, OCCH, D_92
DB 078H, OCCH, 000H, 078H, OCCH, OCCH, 078H, 000H ; D_93
Da 000H, OCCH, 000H, 078H, OCCH, OCCH, 078H, 000H ; D_94
DB 000H, 0E0H, 000H, 078H, OCCH, OCCH, 078H, 000H ; D_95
DB 078H, OCCH, 000H, OCCH, OCCH, OCCH, 07EH, 000H ; D_96
DB 000H, 0E0H, 000H, OCCH, OCCH, OCCH, 07EH, 000H ; D_97
DB 000H, OCCH, 000H, OCCH, OCCH, 07CH, OCCH, 078H ; D_98
DB OC3H, 013H, 03CH, 056H, 066H, 03CH, 018H, 000H ; D_99
DB OCCH, 0F0H, OCCH, OCCH, OCCH, OCCH, 078H, 000H ; D_9A
DB 018H, 018H, 07EH, OCCH, OCCH, 07EH, 018H, 018H ; D_9B
DB 034H, 06CH, 06AH, 0F0H, 060H, 0E6H, 0FCH, 000H ; D_9C
DB 0CCH, 0CCH, 07BH, 0FCH, 030H, 0FCH, 030H, 030H ; D_9D
DB 0F8H, OCCH, OCCH, 0F8H, 06FH, 06FH, OCCH, OCCH, 07EH ; D_9E
DB 00EH, 018H, 018H, 03CH, 018H, 018H, 0D8H, 070H ; D_9F
DB 01CH, 000H, 078H, 00CH, 07CH, OCCH, 07EH, 000H ; D_A0
DB 038H, 000H, 070H, 030H, 030H, 030H, 078H, 000H ; D_A1
DB 000H, 01CH, 000H, 078H, OCCH, OCCH, 078H, 000H ; D_A2
DB 000H, 01CH, 000H, OCCH, OCCH, OCCH, 07EH, 000H ; D_A3
DB 0F8H, 000H, 0F8H, 000H, OCCH, OCCH, OCCH, 07EH ; D_A4
DB 0FCH, 000H, OCCH, 0ECH, 0FCH, 0DCH, 0DCH, 000H ; D_A5
DB 03CH, 06CH, 06CH, 03EH, 000H, 07EH, 900H, 000H ; D_A6
DB 038H, 06CH, 06CH, 038H, 000H, 07CH, 000H, 000H ; D_A7
DB 030H, 000H, 030H, 060H, OCCH, OCCH, 078H, 000H ; D_A8
DB 000H, 000H, 000H, 0FCH, 000H, 000H, 000H, 000H ; D_A9
DB 000H, 000H, 000H, 0FCH, 000H, 000H, 000H, 000H ; D_AA
DB OC3H, 0C6H, OCCH, 0E0H, 033H, 066H, 06FH, 03CH ; D_AC
DB 018H, 018H, 000H, 018H, 018H, 018H, 018H, 000H ; D_AD
DB 000H, 033H, 066H, OCCH, 066H, 033H, 000H, 000H ; D_AF
DB 022H, 088H, 022H, 088H, 022H, 088H, 022H, 088H ; D_B0
DB 055H, 0AAH, 055H, 0AAH, 055H, 0AAH, 055H, 0AAH ; D_B1
DB 0DBH, 077H, 0DBH, 0E0H, 0DBH, 077H, 0DBH, 0E0H ; D_B2
DB 018H, 018H, 018H, 018H, 018H, 018H, 018H, 018H ; D_B3
DB 018H, 018H, 018H, 018H, 018H, 018H, 018H, 018H ; D_B4
DB 036H, 036H, 036H, 036H, 036H, 036H, 036H, 036H ; D_B5
DB 000H, 000H, 000H, 000H, 000H, 000H, 000H, 000H ; D_B6
DB 000H, 000H, 000H, 000H, 000H, 000H, 000H, 000H ; D_B7
DB 000H, 000H, 0F8H, 018H, 0F8H, 018H, 018H, 018H ; D_B8
DB 036H, 036H, 06FH, 006H, 036H, 036H, 036H, 036H ; D_B9
DB 036H, 036H, 036H, 036H, 036H, 036H, 036H, 036H ; D_BA
DB 000H, 000H, 0FEN, 06FH, 036H, 036H, 036H, 036H ; D_BB
DB 036H, 036H, 06FH, 06FH, 036H, 000H, 000H, 000H ; D_BC
DB 036H, 036H, 036H, 036H, 0FEN, 000H, 000H, 090H ; D_BD
DB 018H, 018H, 0F8H, 018H, 0F8H, 018H, 000H, 000H ; D_BE
DB 000H, 000H, 000H, 000H, 000H, 000H, 018H, 018H ; D_BF
DB 018H, 018H, 018H, 018H, 01FH, 000H, 000H, 000H ; D_C0
DB 018H, 018H, 018H, 018H, 0FFH, 000H, 000H, 000H ; D_C1
DB 000H, 000H, 000H, 000H, 0FHH, 018H, 018H, 018H ; D_C2
DB 018H, 018H, 018H, 018H, 01FH, 918H, 018H, 018H ; D_C3
DB 000H, 000H, 000H, 000H, 0FFH, 000H, 000H, 000H ; D_C4
DB 018H, 018H, 018H, 018H, 0FFH, 018H, 018H, 018H ; D_C5
DB 018H, 018H, 01FH, 01FH, 21FH, 018H, 018H, 018H ; D_C6
DB 036H, 036H, 036H, 036H, 036H, 036H, 036H, 036H ; D_C7
DB 036H, 036H, 037H, 030H, 037H, 036H, 036H, 036H ; D_C8
DB 000H, 000H, 03FH, 030H, 037H, 036H, 036H, 036H ; D_C9
DB 036H, 036H, 07FH, 000H, 0FFH, 000H, 000H, 000H ; D_CA
DB 000H, 000H, 0FFH, 000H, 0FFH, 000H, 000H, 000H ; D_CB
DB 036H, 036H, 037H, 030H, 037H, 036H, 036H, 036H ; D_CC
DB 000H, 000H, 0FFH, 000H, 0FFH, 000H, 000H, 000H ; D_CD
DB 036H, 036H, 0F7H, 000H, 0F7H, 036H, 036H, 036H ; D_CE
0678 18 18 FF 00 FF 00 435 DB 018H,018H,0FFH,000H,0FFH,000H,000H ; D_CF
00 00 436 DB 036H,036H,036H,036H,0FFH,000H,000H ; D_D0
0680 36 36 36 36 FF 00 437 DB 000H,000H,0FFH,000H,0FFH,018H,018H ; D_D1
0688 00 00 FF 00 FF 18 440 DB 000H,000H,000H,000H,0FFH,036H,036H ; D_D2
0690 00 00 00 00 FF 36 442 DB 036H,036H,036H,036H,03FH,000H,000H ; D_D3
0698 36 36 36 36 FF 00 444 DB 036H,036H,036H,036H,03FH,000H,000H ; D_D4
06A0 18 1F 18 1F 00 446 DB 018H,018H,01FH,01FH,01FH,000H,000H ; D_D5
06A8 00 1F 18 1F 18 448 DB 000H,000H,01FH,01FH,01FH,018H,018H ; D_D6
06B0 00 00 00 3F 36 450 DB 000H,000H,000H,03FH,036H,036H,036H ; D_D7
06B8 36 36 36 FF 36 452 DB 036H,036H,036H,036H,0FFH,036H,036H ; D_D8
06C0 18 18 18 18 18 454 DB 018H,018H,018H,018H,018H,018H,018H ; D_D9
06C8 18 18 18 18 18 00 456 DB 018H,018H,018H,018H,018H,000H,000H ; D_DA
06D0 00 00 00 00 1F 18 458 DB 000H,000H,000H,000H,01FH,018H,018H ; D_DB
06D8 00 00 00 FF FF 460 DB 0FFH,0FFH,0FFH,0FFH,0FFH,0FFH,0FFH ; D_DC
06E0 FF FF FF FF FF 461 DB 000H,000H,000H,000H,0FFH,0FFH,0FFH ; D_DD
06E8 FO FO FO FO FO 463 DB 0FOH,0FOH,0FOH,0FOH,0FOH,0FOH,0FOH ; D_DE
06F0 OF OF OF OF OF 465 DB 0OFH,0OFH,0OFH,0OFH,0OFH,0OFH,0OFH ; D_DF
06F8 FF FF FF FF 00 467 DB 0FFH,0FFH,0FFH,0FFH,000H,000H,000H ; D_E0
00 00 469 DB 000H,000H,076H,00CH,0CCH,0CCH,076H,00CH ; D_E1
0700 00 76 DC C6 C8 471 DB 000H,078H,0CCH,0F8H,0CCH,0F8H,0CCH,0CCH ; D_E2
0708 78 CC F8 CC F8 473 DB 000H,0FCH,0CCH,0CCH,0CCH,0CCH,0CCH,0CCH ; D_E3
0710 CO CO CO CO CO 475 DB 000H,0FCH,06CH,06CH,06CH,06CH,06CH,06CH ; D_E4
0718 00 FE 6C 6C 6C 477 DB 0FCH,0CCH,060H,030H,060H,0CCH,0FCH,000H ; D_E5
0720 FC 60 30 60 60 479 DB 000H,000H,07EH,008H,008H,070H,000H ; D_E6
0728 00 7E D8 D8 D8 481 DB 000H,066H,066H,066H,066H,07CH,060H,000H ; D_E7
0730 00 66 66 66 7C 483 DB 000H,076H,00CH,018H,018H,018H,018H,000H ; D_E8
0738 00 76 DC 18 18 485 DB 0FCH,030H,078H,0CCH,078H,030H,0FCH ; D_E9
0740 30 7C CC 7C 7C 487 DB 038H,06CH,0C6H,0FEH,0C6H,06CH,038H,000H ; D_EA
0748 38 6C C6 FE C6 489 DB 038H,06CH,0C6H,0C6H,06CH,06CH,0E6H,000H ; D_EB
0750 00 C6 C6 C6 6C 491 DB 01CH,030H,01BH,07CH,0CCH,0CCH,078H,000H ; D_EC
0758 18 7C CC 7C 493 DB 000H,000H,07EH,008H,008H,07EH,000H ; D_ED
0760 00 7E DB D8 495 DB 000H,000H,07EH,008H,008H,07EH,000H ; D_EE
0768 06 0C DB D8 497 DB 006H,00CH,07EH,008H,008H,060H,000H ; D_EF
0770 30 00 FC C0 60 499 DB 038H,060H,0C0H,0F8H,0C0H,060H,038H,000H ; D_F0
0778 38 00 CC CC CC 501 DB 078H,0CCH,0CCH,0CCH,0CCH,0CCH,0CCH,000H ; D_F1
0780 00 FC 00 FC 00 503 DB 000H,0FCH,000H,0FCH,000H,0FCH,000H,000H ; D_F2
0788 30 30 FC 30 30 505 DB 030H,030H,0FCH,030H,030H,0FCH,000H ; D_F3
0790 60 30 18 30 60 507 DB 060H,030H,018H,030H,060H,0FCH,000H ; D_F4
0798 18 30 60 30 18 509 DB 018H,030H,060H,030H,018H,000H,0FCH,000H ; D_F5
07A0 FC 00 18 18 18 511 DB 00EH,01BH,01BH,01BH,01BH,01BH,01BH,01BH ; D_F6
07A8 18 18 18 18 D8 513 DB 018H,018H,018H,018H,018H,088H,008H,070H ; D_F7
07B0 30 30 FC 00 30 515 DB 030H,030H,0FCH,000H,030H,030H,030H,000H ; D_F8
07B8 00 76 DC 00 76 DC 517 DB 000H,076H,00CH,000H,076H,00CH,000H,000H ; D_F9
07C0 0C 6C 6C 38 00 519 DB 038H,06CH,06CH,038H,000H,000H,000H,000H ; D_FA
07C8 00 00 18 18 18 521 DB 000H,000H,018H,018H,000H,000H,000H,000H ; D_FB
07D0 00 00 00 18 18 523 DB 000H,000H,000H,000H,018H,000H,000H,000H ; D_FC
07D8 0F OC OC EC 6C 525 DB 0FFH,0CCH,0CCH,0CCH,0CCH,06CH,06CH,000H ; D_FD
07E0 78 6C 6C 6C 6C 00 527 DB 078H,06CH,06CH,06CH,06CH,06CH,000H,000H ; D_FE
07E8 00 00 30 60 78 00 529 DB 000H,000H,030H,060H,078H,000H,000H,000H ; D_FF
07F0 00 3C 3C 3C 3C 531 DB 000H,000H,03CH,03CH,03CH,03CH,000H,000H ; D_FG
07F8 00 00 00 00 00 533 DB 000H,000H,000H,000H,000H,000H,000H,000H ; D_FH
0800 00 00 535 DB 000H,000H,000H,000H,000H,000H,000H,000H ; D_FI
0800 CODE ENDS END

PAGE 120
SUBTITL END ADDRESS
0000 CODE SEGMENT PUBLIC
0000 PUBLIC CODE_ADDRESS
0000 END_ADDRESS LABEL BYTE
0000 CODE ENDS END
Index

A
Attribute Address Register 56
Attribute Controller
    description 3
    registers 56
compatibility issues 74
configuration switches 80
CRT Controller
    description 3
    registers 24
CRT Controller Address Register 24
CRT Controller Overflow Register 30
Cursor End Register 33
Cursor Location High Register 35
Cursor Location Low Register 35
Cursor Start Register 32

B
BIOS
    description 4
    vectors with special meanings 103
    BIOS listing 103
    Bit Mask Register 54

C
character generator
    ROM 1
Character Map Select Register 21
Clocking Mode Register 19
Color Compare Register 48
Color Don't Care Register 53
color mapping 10
Color Plane Enable Register 60

D
Data Rotate Register 49
direct drive connector 83
display buffer 4

E
Enable Set/Reset Register 47
End Horizontal Blanking Register 27
End Horizontal Retrace Register 29
End Vertical Blanking Register 40

F
feature connector 76
Feature Control Register 14

G
Graphics Controller description 3 registers 45
Graphics 1 and 2 Address Register 46
Graphics 1 Position Register 45
Graphics 2 Position Register 46

H
Horizontal Display Enable End Register 26
Horizontal Pel Panning Register 60
Horizontal Total Register 25

I
Input Status Register One 15
Input Status Register Zero 14
Interface 76
    feature connector 76

L
Light Pen High Register 36
light pen interface 84
Light Pen Low Register 37
Line Compare Register 43

M
Map Mask Register 20
Maximum Scan Line Register 32
Memory Mode Register 23
Miscellaneous Output Register 12
Miscellaneous Register 52
Mode Control Register 41, 58
Mode Register 50
modes
    alphanumeric 8
    graphics 8
    IBM Color Display 5
    IBM Enhanced Color Display 6
    IBM Monochrome Display 6
O
Offset Register 38
Overscan Color Register 59

P
Palette Registers 57
Preset Row Scan Register 31
programming
considerations 62
compatibility issues 74
creating a split screen 73
creating a 512 character set 70
creating an 80 by 43 alphanumeric mode 71
programming registers 62
RAM loadable character generator 69
vertical interrupt feature 72

R
RAM loadable character generator 69
Read Map Select Register 50
registers
Attribute Controller 56
CRT Controller 24
external 12
Graphics Controller 45
Sequencer 18
Reset Register 18

S
Sequencer
description 3
registers 18
Sequencer Address Register 18
Set/Reset Register 47
specifications 79
configuration switch settings 81
configuration switches 80
direct drive connector 83
light pen interface 84
system board switches 79
Start Address High Register 34
Start Address Low Register 34
Start Horizontal Blanking Register 26
Start Horizontal Retrace Pulse Register 28
Start Vertical Blanking Register 39
support logic 4

U
Underline Location Register 39
V

Vertical Display Enable End Register 38
vertical interrupt feature 72

Vertical Retrace End Register 36
Vertical Retrace Start Register 36
Vertical Total Register 30

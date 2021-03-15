# knot

**Knot is a DIY Asset Tracking device to mitigate STOLEN or LOST property by tracking them with cellular (GSM 1400) and GPS.**

I wanted some way to track my bike, but couldn't find any really great options available on the internet. I figured it would be pretty easy to build one off-the-shelf using the Arduino MKR GSM 1400 that I could customize and integrate into my bike, and that's what this project is. The primary goal of this project is just to build a device I trust to track my bike. The secondary goals for this project are to keep this as simple and generic as possible, and make it easy to extend. This is a completely homespun IOT system (including drivers and server code) which is hopefully lightweight enough for anyone to use and modify for their own uses.

## Design

It's basically just an Arduino MKR GSM 1400 and GPS shield shoved in a 3d-printed case tweaked so both antennas can get consistent signal in good signal areas, and designed to mount on a bike. Including a press-fit cavity for a micro-usb charging head as well inside of the bolted down case to allow for power to be routed to the module from an external supply (bike battery) that can't just be pulled out. Technically the wire can still be cut rather trivially if the wire isn't also covered, but this just adds an extra guard. Also adding a switch (and possibly also light/sound/haptic elements) to allow user to switch bike into alarm mode to sense if it's moved and send warning texts. I don't think the Arduino has an IMU built in so planning on just using GPS ping deltas to detect movement, although a cheap IMU module may provide more accurate performance in this area. I also want to be able to ping the device and ask for a location remotely. This could all be done over text without an external overwatch (still figuring this aspect out. May be cheaper to do just over internet, and has added ability of providing last-known coverage in case of cellular blackouts or disconnect)

## Ping Schedule
- Ping user at some regular frequency if GPS delta has changed at all
- Ping in response to user message sent to device (is there a number associated with?)
- Ping in response to bike being moved in track movement mode.

If I add an intermediary server layer in, can get rid of direct message (mediate through server), and can send regular pings to server to log in dynamo instead of to user. Likely comes down to pricing for each approach.

## Parts List
- (1x) Arduino MKR GSM 1400
- (1x) Arduino MKR GPS shield
- (1x) micro-usb charging cable (optional, for splicing to external power supply)
- (1x) SIM Card with at least local coverage (I'm using the "free" Hologram Pilot https://www.hologram.io/)

Unsourced:
- LiPo battery w/ adapter (internal / back-up battery source)
- Antenna (currently using 5-in-1 that comes with MKR GSM 1400)
- Antenna for GPS? (Think module has one built in, not sure)
- Buttons / switches for mode toggles ?
- Speaker / buzzer ?
- Lights ?
- mounting hardware (will likely make this modular to give users options. I'm planning on using this with my RadMission bike so will likely design a frame mount specifically designed for this to improve my particular bike security. The main body may be designed to drop into a variety of these mounting solutions if other people want to build this for their uses.) 

*NOTE: that some of the Rad Power Bikes or other power bikes come with built in USB charging. This may be a better option for users that don't want to mess with their bike's controller electronics to splice the Arduino into the bike battery power supply since the Arduino can be powered by USB*

You'll also need a 3d printer or access to one to print some of the housing elements.

## Software
### Driver
Arduino MKR GSM 1400 Drivers

### AWS?
Not sure how / if I want a server component. This would be it if I choose to (either lambda w/ API gateway or EC2)

W/ lambda and dynamodb, it's essentially free at my volumes, and api gateway is around $3.50 / 333 million requests, so also very cheap. SNS may also be a good option for message sending.

### Client?
React web app for interfacing with a tracker. Not sure if I need it yet, but if I do will probably just be a device map


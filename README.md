Hall is a spatialization tool designed to create highly directional reverb sends. 

This prototype is not technically a proper reverb, but the filtered delays help simulate the use of the plugin. Uses basic stereo to create
percieved local effect and direction. This is an idea I've had for a while and tried a quick implementation to see if it would be as fun 
as I imagined. 

Direction: The Big Knob in the middle

The arrow indicates the direction and the reverb tail is sent. 9 o'clock is left to right, 11 o'clock is front left to behind to the right

Reverb Shaping Controls:

Delay: controls the delay length before reverb starts, effectively "predelay"

Decay: controls the length of the reverb tail

Mix: basic dry/wet control

Tone: a basic low pass filter to soften the timbre of the reverb

Width: (WIP) controls the starting position of the verb. Essentially distance or proximity, the higher the width param the further from the mono center or listener the source is, in the opposite direction of the tail angle (direction wheel param value). Basically, if width is 0, the reverb covers the radius of the wheel. If width is max, it covers the diameter.

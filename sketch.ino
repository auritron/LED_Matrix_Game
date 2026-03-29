int gameMatrix[5][12][2]; //initialize matrix that directly maps onto the LED arrays
uint8_t tick; //tick represents the smallest unit of time where the game code updates
const int tick_delay = 50; //minimum tick delay (i.e. delay between two ticks)
const int carAdvFreq = 10; //frequency for car advancement; default delay = 500ms
const int laneGenFreq = 30; //frequency for lane generation; default delay = 1500ms
const int resetTicks = 40; //no. of ticks for game reset; default delay = 2000ms
const int resetFlickerFreq = 10; //interval of reset flash; default interval = 500ms
const int carSpawnChance = 7; //chance of a car spawning in a lane, default is 70%

//player
int player_y; //player y value (b/w 0 to 4, corresponds to rows w/ 0 being the top row and 4 being the bottom)
int upButtonOldVal; //for up button press detection
int downButtonOldVal; //for down button press detection
bool movedUp; //for checking if up button pressed already
bool movedDown; //for checking if down button press already
bool resetPressed; //for checking if reset button is pressed already
bool reset; //when true, the game resets, and tick is reset to zero
bool game_setup = true; //when true, game goes into setup mode and (re)intializes variables

//score system
int score; //score
int high_score = 0; //high score
bool scorePrinted = false; //if score has already been printed
bool lanePresentOld = false; //if a player passed a lane (old val)

const int dataPin = 10; //send data to shift register
const int clrPin = 11; //reset the memory of shift registers
const int clkPin = 12; //clock to load data
const int latchPin = 13; //clock to output data from driver to LED
const int resetPin = 4; //pin to reset the game
const int OEPin = 5; //pin to clear output but preserve register memory
const int upButtonPin = 7; //player go up
const int downButtonPin = 6; //player go down

//flattens the array from 5x12x2 3D array -> 15 element long 1D array (each with elements that are 8 bits long) to send all the bits serially to the registers
void flattenArray(int inputArray[5][12][2], uint8_t newArr[15]) {
  int byteIndex = 0;
  int bitNo = 0;
  newArr[byteIndex] = 0;

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 12; j++) {
      for (int k = 0; k < 2; k++) {
        newArr[byteIndex] <<= 1; //shift to left one space
        newArr[byteIndex] |= inputArray[i][j][k]; //perform bitwise OR to add bit as the LSB of the string
        bitNo++; //increase the number of bits every time a new bit is added

        if (bitNo == 8) {
          byteIndex++; //increase the number of bytes
          bitNo = 0; //reset bit count to zero, as it shifts to the next byte/element in the array
          if (byteIndex < 15) {
            newArr[byteIndex] = 0;
          }
        }
      }
    }
  }
}

//update element 0 and 1 in the inner most 2 element array of GameMatrix, to chagne the color
void updatePixel(int row, int column, bool overrideRed, bool isRed, bool overrideGreen, bool isGreen) {
  //overrideRed/Green will change red/green value too, if false then it will not override and preserve R/G value
  if (overrideRed) {
    gameMatrix[row][column][0] = (int) isRed;
  } if (overrideGreen) {
    gameMatrix[row][column][1] = (int) isGreen;
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));

  //pin initialization
  pinMode(upButtonPin, INPUT_PULLUP); //up button
  pinMode(downButtonPin, INPUT_PULLUP); //down button
  pinMode(resetPin, INPUT_PULLUP); //reset game button
  pinMode(dataPin, OUTPUT); //send data
  pinMode(clrPin, OUTPUT); //clear register memory
  pinMode(clkPin, OUTPUT); //update into register memory (clock)
  pinMode(latchPin, OUTPUT); //update register memory into output (clock)
  pinMode(OEPin, OUTPUT); //clear output of register without erasing memory

  digitalWrite(clrPin, HIGH); //make sure clear register memory pin is HIGH (where HIGH is memory will not be cleared)
  digitalWrite(OEPin, LOW); //make sure output enable is LOW (where LOW means output will not be off)
}

void loop() {
  // // put your main code here, to run repeatedly:

  //game setup
  if (game_setup) { //only execute when game_setup is true
    for (int i = 0; i < 5; i++) {
      for (int j = 0; j < 12; j++) {
        updatePixel(i, j, 1, 0, 1, 0); //clear all pixels to OFF
      }
    }
    updatePixel(2, 3, 0, 0, 1, 1); //initialize player pixel (GREEN)
    player_y = 2; //player starts in second row
    upButtonOldVal = LOW;
    downButtonOldVal = LOW;
    movedUp = false;
    movedDown = false;
    resetPressed = false;
    reset = false; //set reset to off so that game does not get stuck on resetting
    score = 0;
    scorePrinted = false;

    tick = 0; //set tick to 0 at the starting, default tick speed is 20 ticks/s (or) 1 tick/50ms
    game_setup = false; //set game_setup to false so that code doesnt run unless game starts/restarts
  }

  int no_of_cars = 0; //no of cars in a lane (max is 4)

  //to update button presses
  if (!reset) { //this code make sure that holding the button does not trigger with every tick, and only triggers again once the button is set back to OFF and then pressed again
    int upButtonNewVal = digitalRead(upButtonPin);
    int downButtonNewVal = digitalRead(downButtonPin);

    if(upButtonNewVal != upButtonOldVal)  {
      upButtonOldVal = upButtonNewVal;
    }
    if(downButtonNewVal != downButtonOldVal)  {
      downButtonOldVal = downButtonNewVal;
    }

    //same as above for reset, reset button resets the game completely
    int resetState = digitalRead(resetPin);
    if (resetState == LOW && !resetPressed) {
      reset = true;
      gameMatrix[player_y][3][0] = 1;
      tick = 0;
      resetPressed = true;
    }
    if (resetState == HIGH && resetPressed) {
      resetPressed = false;
    }

    //update on release
    if (upButtonNewVal) movedUp = false;
    if (downButtonNewVal) movedDown = false;

    //player movement updation
    updatePixel(player_y, 3, 0, 0, 1, 0); //set player pixel to OFF by default
    if(!upButtonNewVal && !movedUp) { //if player is moving up, update pixel position by decrementing y value (as higher y value means a row to the bottom, lower y value is row to the top)
      if (player_y <= 0) { //if player is at the top, going up sends the player to the bottom
        player_y = 4;
      } else {
        player_y--;
      }
      movedUp = true;
    } else if (!downButtonNewVal && !movedDown){ //increments the y value to move downwards
      if (player_y >= 4) { //if player is at the bottom, going down sends the player to the top
        player_y = 0;
      } else {
        player_y++;
      }
      movedDown = true;
    }
    updatePixel(player_y, 3, 0, 0, 1, 1); //update the new pixel with the player y position (if the player didn't move in that tick, their pixel is set back to ON (GREEN) again)
  }

  //check for collision
  if (gameMatrix[player_y][3][0] == 1) { //checks if player and car have the same position every tick (i.e. collision)
    if (!reset) { //resets tick if collision detected
      tick = 0;
    }
    reset = 1; //set reset to one indicating game over and reset for a new attempt
    //Serial.println("Collision Detected!");
    for (int row = 0; row < 5; row++) { //clears all pixels to off EXCEPT the player's pixel
      for (int column = 0; column < 12; column++) {
        updatePixel(row, column, 1, 0, 0, 0);
      }
    }
    updatePixel(player_y, 3, 1, 1, 1, 0); //player's color changes to red for death animation
  }

  if (reset) {
    if (!scorePrinted) {
      for (int i = 0; i < 100; i++) { //print multiple newlines to 'clear' Serial Monitor
        Serial.println();
      }
      Serial.print("You got hit!\nScore: ");
      Serial.println(score);
      Serial.print("High Score: ");
      Serial.println(high_score);
      scorePrinted = true; //prevent score from being printed multiple times
    }
    if (tick % (resetFlickerFreq * 2) < resetFlickerFreq) { //player color flashes between ON (RED) and OFF 2 times in 0.5 second intervals as death animation
      digitalWrite(OEPin, LOW);
    } else {
      digitalWrite(OEPin, HIGH);
    }
    if (tick % (resetTicks) == 0 && tick > 0) { //after number off resetTicks, the game is reset and can be attempted again
      game_setup = true;
    }
  }

  if (!reset) {
    //car advancement and score incrementation
    bool lanePresentNew = false;

    if(tick % carAdvFreq == 0) {
      for (int row = 0; row < 5; row++) { //do this for every row
        //score checking
        if (gameMatrix[row][3][0] == 1) { //check if a new lane is approaching
          lanePresentNew = true;
        }
      }
      
      if (lanePresentOld && !lanePresentNew) { //check if previously a lane was there in the players lane, and if its not there now then the player dodged it
        score++; //increment score
        if (score > high_score) { //increase high score
          high_score = score;
        }
      }

      lanePresentOld = lanePresentNew; //updatae old value for lane present
      
      for (int row = 0; row < 5; row++) {
        for (int column = 0; column < 11; column++) {
          gameMatrix[row][column][0] = gameMatrix[row][column + 1][0]; //shifts the row of the matrix to the left, making the red pixels (cars) move one space to the left
        }
        updatePixel(row, 11, 1, 0, 0, 0); //update the last column which is now empty with 0 to indicate an empty column
      }
    }

    //lane generation
    if (tick % laneGenFreq == 0) {
      for (int i = 0; i < 5; i++) {
        updatePixel(i, 11, 1, 0, 1, 0);  //update to 0 first
        if (no_of_cars < 4) { //will not spawn a new car in the lane if number of cars equals 4, making sure the game remains possible
            int randVal = random(1, 11);
            //Serial.println(randVal);
            if (randVal <= carSpawnChance) { //7 is default value, if randval is less than/equal to than default chance, then car will spawn in that cell
                updatePixel(i, 11, 1, 1, 1, 0); //update to 1 if randval
                no_of_cars++;
            }
        } else {
            break;
        }
      }
    }
  }

  //display code
  uint8_t displayData[15]; //initializes empty 15 element array with 8 bits each for every element
  flattenArray(gameMatrix, displayData); //flattens gamematrix and sends the flattened array into display data
  digitalWrite(latchPin, LOW); //set latchpin to low to make sure display doesnt get updated while shifting

  //shift out all bits from the flattened array serially into LED matrix
  for (int i = 0; i < 15; i++) {
    shiftOut(dataPin, clkPin, LSBFIRST, ~displayData[14 - i]);
  }
  digitalWrite(latchPin, HIGH); //update all pixels simultaneously
  
  tick++; //increment tick
  delay(tick_delay); //master delay between each tick
}

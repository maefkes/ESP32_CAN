#include <Arduino.h>
#include "bg_task.h"
#include "application.hpp"


void setup() 
{
  Serial.begin(115200);
  bg_task_init();
  app_init();
}

void loop() 
{
  bg_task_cyclic();
}
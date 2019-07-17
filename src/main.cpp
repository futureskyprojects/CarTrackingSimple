#include <CarTracking.h>

void setup()
{
  setupModule();
}
void loop()
{
  waitForSim808WakeUp(6);
  getGpsData();
  sendGpsData();
}
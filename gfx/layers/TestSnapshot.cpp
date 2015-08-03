#include "TestSnapshot.h"
#include <stdlib.h>
#include "cutils/properties.h"

bool UseWidgetLayer()
{
  char propValue[PROPERTY_VALUE_MAX];
  property_get("bignose.widget", propValue, "1");

  return atoi(propValue);
}

bool UseDirectTexture()
{
  char propValue[PROPERTY_VALUE_MAX];
  property_get("bignose.direct.texture", propValue, "0");

  return atoi(propValue);
}

bool UseOriginalPath()
{
  char propValue[PROPERTY_VALUE_MAX];
  property_get("bignose.original", propValue, "0");

  return atoi(propValue);
}


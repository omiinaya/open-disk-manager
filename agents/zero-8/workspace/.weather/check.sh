#!/bin/bash
# Weather monitor for Pompano Beach, FL
# Alerts on: rain, snow, thunderstorms, temp > 35°C or < 5°C, wind > 50 km/h

LOC="Pompano+Beach"
LAST_STATE="/root/.openclaw/agents/zero/workspace/.weather/last_state"
ALERT_LOG="/root/.openclaw/agents/zero/workspace/.weather/alerts.log"

DATA=$(curl -s "wttr.in/${LOC}?format=j1" 2>/dev/null)
if [ -z "$DATA" ]; then
  exit 1
fi

# Extract current conditions
TEMP=$(echo "$DATA" | jq -r '.current_condition[0].temp_C')
WEATHER=$(echo "$DATA" | jq -r '.current_condition[0].weatherDesc[0].value')
WIND=$(echo "$DATA" | jq -r '.current_condition[0].windspeedKmph')
CHANCE_RAIN=$(echo "$DATA" | jq -r '.weather[0].hourly[4].chanceofrain')
CHANCE_THUNDER=$(echo "$DATA" | jq -r '.weather[0].hourly[4].chanceofthunder')

STATE="${TEMP}|${WEATHER}|${WIND}|${CHANCE_RAIN}"

if [ -f "$LAST_STATE" ]; then
  LAST=$(cat "$LAST_STATE")
  if [ "$STATE" != "$LAST" ]; then
    echo "[$(date)] Weather changed: $LAST -> $STATE" >> "$ALERT_LOG"
  fi
fi

echo "$STATE" > "$LAST_STATE"

# Alert conditions
ALERT=0
MSG=""

if [ "$CHANCE_RAIN" -gt 50 ]; then
  MSG="Rain likely: ${CHANCE_RAIN}%"
  ALERT=1
fi

if [ "$CHANCE_THUNDER" -gt 30 ]; then
  MSG="Thunder possible: ${CHANCE_THUNDER}%"
  ALERT=1
fi

if [ "$TEMP" -gt 35 ]; then
  MSG="Heat warning: ${TEMP}°C"
  ALERT=1
fi

if [ "$TEMP" -lt 5 ]; then
  MSG="Cold warning: ${TEMP}°C"
  ALERT=1
fi

if [ "$WIND" -gt 50 ]; then
  MSG="High wind: ${WIND} km/h"
  ALERT=1
fi

if [ "$ALERT" -eq 1 ]; then
  echo "[$(date)] ALERT: $MSG" >> "$ALERT_LOG"
fi

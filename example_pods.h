Pod *pods[] = {
  new MotionPod("inside", POD_PIN(D7)),
  new MotionPod("outside", POD_PIN(D8)),
  new DoorLatchPod("door", POD_PIN(D6)),
  new DoorSensorPod("door", POD_PIN(D5)),
  NULL
};

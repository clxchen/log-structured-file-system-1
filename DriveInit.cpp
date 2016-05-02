#include "Lab7.h"

void makeDriveDir(){
  struct stat st = {0};

  if (stat("./DRIVE", &st) == -1)
    mkdir("./DRIVE", 0700);
}

void initSegments(){
  std::ofstream outs[NO_SEGMENTS];
  for (int i = 0; i < NO_SEGMENTS; ++i){
    std::string filename = "DRIVE/SEGMENT" + std::to_string(i+1);
    outs[i].open(filename, std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);

    char true_zero = 0;
    for (int j = 0; j < SEG_SIZE; ++j) outs[i] << true_zero;

    outs[i].close();
  }
}

void initCheckpointRegion(){
  std::ofstream out;
  out.open("DRIVE/CHECKPOINT_REGION", std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);

  char true_zero = 0;
  for (int j = 0; j < IMAP_BLOCKS; ++j) out << true_zero << true_zero << true_zero << true_zero;

  out.close();
}

void initFilenameMap(){
  std::ofstream out;
  out.open("DRIVE/FILENAME_MAP", std::ofstream::out | std::ofstream::trunc);

  out.close();
}

int main(int argc, char const *argv[]){
  makeDriveDir();
  initSegments();
  initCheckpointRegion();
  initFilenameMap();
  printf("%s\n", "Drive created.");
  return 0;
}
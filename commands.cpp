#include "functions.cpp"

void import(std::string filename, std::string lfs_filename) {
  std::ifstream in(filename);
  if (!in.good()){
    std::cout << "Could not find file." << std::endl;
    return;
  }

  //get input file length
  in.seekg(0, std::ios::end);
  int in_size = in.tellg();
  in.seekg(0, std::ios::beg);

  if ((in_size / BLOCK_SIZE) + 3 > ASSIGNABLE_BLOCKS - AVAILABLE_BLOCK)
    writeOutSegment();

  if (lfs_filename.length() > 254){
    std::cout << "Filename too large." << std::endl;
    in.close();
    return;
  }

  if (getInodeNumberOfFile(lfs_filename) != (unsigned int) -1){
    std::cout << "Duplicate filename." << std::endl;
    in.close();
    return;
  }

  unsigned int inode_number = nextInodeNumber();

  if (inode_number == -1) {
    std::cout << "Max files reached." << std::endl;
    in.close();
    return;
  }
  updateFilemap(inode_number, lfs_filename);

  //read from file we're importing and write it in blocks of SEGMENT
  char buffer[in_size];
  in.read(buffer, in_size);
  std::memcpy(&SEGMENT[AVAILABLE_BLOCK * BLOCK_SIZE], buffer, in_size);

  //inode blocks
  inode meta;
  for (int i = 0; i < lfs_filename.length(); ++i){
    meta.filename[i] = lfs_filename[i];
  }
  meta.filename[lfs_filename.length()] = '\0';
  meta.size = in_size;
  for (unsigned int i = 0; i <= in_size/BLOCK_SIZE; ++i){
    meta.block_locations[i] = AVAILABLE_BLOCK + (SEGMENT_NO-1)*BLOCKS_IN_SEG;
    SEGMENT_SUMMARY[AVAILABLE_BLOCK][0] = inode_number;
    SEGMENT_SUMMARY[AVAILABLE_BLOCK][1] = i;
    AVAILABLE_BLOCK++;
  }

  writeInode(meta, inode_number);

  //update imap (which also updates checkpoint region)
  updateImap(inode_number, (AVAILABLE_BLOCK - 1) + (SEGMENT_NO-1)*BLOCKS_IN_SEG);

  //for (int i = AVAILABLE_BLOCK - meta.size/BLOCK_SIZE - 3; i < AVAILABLE_BLOCK; ++i)
    //printf("SUMMARY[%d]: {%u, %u}\n", i+(SEGMENT_NO-1)*BLOCKS_IN_SEG, SEGMENT_SUMMARY[i][0], SEGMENT_SUMMARY[i][1]);

  in.close();
}

void remove(std::string lfs_filename) {
  unsigned int inode_number = getInodeNumberOfFile(lfs_filename);

  if (inode_number == (unsigned int) -1){
    std::cout << "Could not find file." << std::endl;
    return;
  }

  std::fstream filemap("DRIVE/FILEMAP", std::ios::binary | std::ios::in | std::ios::out);

  filemap.seekp(inode_number*FILEMAP_BLOCK_SIZE);
  filemap.write(INVALID, 1);

  filemap.close();

  updateImap(inode_number, (unsigned int) -1);

  //printf("SUMMARY[%d]: {%u, %u}\n", AVAILABLE_BLOCK - 1, SEGMENT_SUMMARY[AVAILABLE_BLOCK - 1][0], SEGMENT_SUMMARY[AVAILABLE_BLOCK - 1][1]);
}

void cat(std::string lfs_filename) {
  unsigned int inode_number = getInodeNumberOfFile(lfs_filename);

  if (inode_number == (unsigned int) -1){
    std::cout << "Could not find file." << std::endl;
    return;
  }

  inode meta = getInode(inode_number);

  int no_data_blocks = (meta.size / BLOCK_SIZE) + 1;

  for (int i = 0; i < no_data_blocks; ++i)
    printBlock(meta.block_locations[i], 0, meta.size, (i == 0), (i == no_data_blocks - 1));
}

void display(std::string lfs_filename, std::string amount, std::string start) {
  unsigned int inode_number = getInodeNumberOfFile(lfs_filename);

  if (inode_number == (unsigned int) -1){
    std::cout << "Could not find file." << std::endl;
    return;
  }

  inode meta = getInode(inode_number);

  int start_byte = std::stoi(start);
  int end_byte = std::stoi(amount) + start_byte;

  if (start_byte > meta.size) {
    printf("Cannot specify a start byte greater than filesize.\n");
    return;
  }

  if (end_byte > meta.size) {
    printf("\t\t   ============WARNING============\n\tAmount specified too great, displaying until end of file.\n");
    end_byte = meta.size;
  }

  for (int i = start_byte/BLOCK_SIZE; i <= end_byte/BLOCK_SIZE; ++i)
    printBlock(meta.block_locations[i], start_byte, end_byte, (i == start_byte/BLOCK_SIZE), (i == end_byte/BLOCK_SIZE));
}

void overwrite(std::string lfs_filename, std::string amount_string, std::string start, std::string character_string) {
  int amount = std::stoi(amount_string);
  int start_byte = std::stoi(start);
  int end_byte = amount + start_byte;
  char character = character_string.c_str()[0];

  unsigned int inode_number = getInodeNumberOfFile(lfs_filename);

  if (inode_number == (unsigned int) -1){
    std::cout << "Could not find file." << std::endl;
    return;
  }

  inode meta = getInode(inode_number);

  if (start_byte > meta.size) {
    printf("Cannot specify a start byte greater than filesize.\n");
    return;
  }

  if(end_byte > MAX_DATA_BLOCKS*BLOCK_SIZE){
    printf("Cannot specify a character amount greater than the max filesize.\n");
    return;
  }

  int start_block = start_byte / BLOCK_SIZE;
  int end_block = end_byte / BLOCK_SIZE;
  int blocks_needed = end_block - start_block + 3;

  if (blocks_needed > ASSIGNABLE_BLOCKS - AVAILABLE_BLOCK)
    writeOutSegment();

  char repeated_chars[amount];
  for (int i = 0; i < amount; ++i)
    repeated_chars[i] = character;

  //copy some of the first block
  int start_block_seg = (meta.block_locations[start_block] / BLOCKS_IN_SEG) + 1;
  if (start_block_seg != SEGMENT_NO){
    char buffer[start_byte % BLOCK_SIZE];
    std::fstream start_block_seg_file("DRIVE/SEGMENT"+std::to_string(start_block_seg), std::ios::in | std::ios::out | std::ios::binary);
    start_block_seg_file.seekg((meta.block_locations[start_block] % BLOCKS_IN_SEG) * BLOCK_SIZE);
    start_block_seg_file.read(buffer, start_byte % BLOCK_SIZE);
    start_block_seg_file.close();
    std::memcpy(&SEGMENT[AVAILABLE_BLOCK * BLOCK_SIZE], buffer, start_byte % BLOCK_SIZE);
  }else{
    std::memcpy(&SEGMENT[AVAILABLE_BLOCK * BLOCK_SIZE], &SEGMENT[(meta.block_locations[start_block] % BLOCKS_IN_SEG) * BLOCK_SIZE], start_byte % BLOCK_SIZE);
  }

  //copy the characters
  std::memcpy(&SEGMENT[AVAILABLE_BLOCK * BLOCK_SIZE + start_byte], repeated_chars, amount);

  //perhaps copy some of the last block
  if (end_byte < meta.size){
    int end_block_seg = (meta.block_locations[end_block] / BLOCKS_IN_SEG) + 1;
    if (end_block_seg != SEGMENT_NO){
      char buffer[BLOCK_SIZE - end_byte];
      std::fstream end_block_seg_file("DRIVE/SEGMENT"+std::to_string(end_block_seg), std::ios::in | std::ios::out | std::ios::binary);
      end_block_seg_file.seekg((meta.block_locations[end_block] % BLOCKS_IN_SEG) * BLOCK_SIZE + end_byte);
      end_block_seg_file.read(buffer, BLOCK_SIZE - end_byte);
      end_block_seg_file.close();
      std::memcpy(&SEGMENT[AVAILABLE_BLOCK * BLOCK_SIZE + end_byte], buffer, BLOCK_SIZE - end_byte);
    }else{
      std::memcpy(&SEGMENT[AVAILABLE_BLOCK * BLOCK_SIZE + end_byte], &SEGMENT[(meta.block_locations[end_block] % BLOCKS_IN_SEG) * BLOCK_SIZE + (end_byte % BLOCK_SIZE)], BLOCK_SIZE - (end_byte % BLOCK_SIZE));
    }
  }else{
    meta.size = end_byte;
  }

  for (int i = start_block; i <= end_block; ++i){
    meta.block_locations[i] = AVAILABLE_BLOCK + (SEGMENT_NO-1) * BLOCKS_IN_SEG;
    SEGMENT_SUMMARY[AVAILABLE_BLOCK][0] = inode_number;
    SEGMENT_SUMMARY[AVAILABLE_BLOCK][1] = i;
    AVAILABLE_BLOCK++;
  }

  writeInode(meta, inode_number);

  updateImap(inode_number, (AVAILABLE_BLOCK - 1) + (SEGMENT_NO-1)*BLOCKS_IN_SEG);

  //for (int i = AVAILABLE_BLOCK - blocks_needed; i < AVAILABLE_BLOCK; ++i)
    //printf("SUMMARY[%d]: {%u, %u}\n", i+(SEGMENT_NO-1)*BLOCKS_IN_SEG, SEGMENT_SUMMARY[i][0], SEGMENT_SUMMARY[i][1]);
}

void list() {
  std::cout << "Current Segment: " << SEGMENT_NO << std::endl;
  std::ifstream filemap("DRIVE/FILEMAP");

  for (int i = 0; i < MAX_FILES; ++i){
    filemap.seekg(i*FILEMAP_BLOCK_SIZE);
    char valid[1];
    filemap.read(valid, 1);

    if (valid[0]) {
      char filename[FILEMAP_BLOCK_SIZE-1];
      filemap.read(filename, FILEMAP_BLOCK_SIZE-1);

      printf("%s %d\n", filename, getFileSize(i));
    }
  }

  filemap.close();
}

void clean(std::string amount_string) {
  int amount = std::stoi(amount_string);
  char segments_to_clean[amount];
  int no_segments_to_clean = 0;

  for (int i = 0; i < NO_SEGMENTS && no_segments_to_clean < amount; ++i) {
    if (CLEAN_SEGMENTS[i] == DIRTY)
      segments_to_clean[no_segments_to_clean++] = i+1;
  }

  if (no_segments_to_clean == 0){
    printf("There are no dirty segments to clean.\n");
    return;
  } else if (no_segments_to_clean < amount) {
    printf("Not enough dirty segments available.\n");
    amount = no_segments_to_clean;
  }

  unsigned int clean_summary[BLOCKS_IN_SEG][2];
  for (int i = 0; i < BLOCKS_IN_SEG; ++i) {
    for (int j = 0; j < 2; ++j)
      clean_summary[i][j] = (unsigned int) -1;
  }

  char clean_segment[ASSIGNABLE_BLOCKS * BLOCK_SIZE];
  unsigned int next_available_block_clean = 0;
  int clean_segment_no = segments_to_clean[0];
  std::vector<inode> inodes;
  std::set<int> fragments;

  if (amount > 1) printf("Cleaning %d segments...\n", amount);
  else            printf("Cleaning 1 segment...\n");

  for (int i = 0; i < amount; i++){
    CLEAN_SEGMENTS[segments_to_clean[i]] = CLEAN;
    printf("Cleaning Segment %d...\n", segments_to_clean[i]);
    cleanSegment(segments_to_clean[i], clean_summary, clean_segment, next_available_block_clean, clean_segment_no, inodes, fragments);
  }

  writeCleanSegment(clean_summary, clean_segment, next_available_block_clean, clean_segment_no, inodes, fragments);

  findNextAvailableBlock();
  readInSegment();
}

void exit() {
  writeOutSegment();
  writeOutCheckpointRegion();
  exit(0);
}

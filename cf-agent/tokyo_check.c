#include "tokyo_check.h"

#ifdef TCDB
/*
 * The idea behind the following code comes from : copiousfreetime@github
 */

/*
 * node for holding offset information and for correlating data
 */
typedef struct rbtree {
  uint64_t offset;      /* the offset in the file */
  int64_t bucket_index; /* the index into the bucket list this offset exists */

  struct rbtree *left;
  struct rbtree *right;
  char color_field;
} rbtree;

/*
 * Fields we will need from a TC record
 */
typedef struct tcrec {
  uint64_t offset;
  uint64_t length;

  uint64_t left;
  uint64_t right;

  uint32_t key_size;
  uint32_t rec_size;
  uint16_t pad_size;

  uint8_t  magic;
  uint8_t  hash;
} tcrec;


static inline int db_offset_comparator( rbtree* a, rbtree* b )
{
  if ( a->offset < b->offset ) {
    return -1;
  } else if ( a->offset > b->offset ) {
    return 1;
  } else {
    return 0;
  }
}

SGLIB_DEFINE_RBTREE_PROTOTYPES(rbtree, left, right, color_field, db_offset_comparator);
SGLIB_DEFINE_RBTREE_FUNCTIONS(rbtree, left, right, color_field, db_offset_comparator);

/* meta information from the Hash Database
 * used to coordinate the other operations
 */
typedef struct db_meta {
  uint64_t bucket_count;         /* Number of hash buckets                */
  uint64_t bucket_offset;        /* Start of the bucket list              */

  uint64_t record_count;         /* Number of records                     */
  uint64_t record_offset;        /* First record  offset in file          */

  short    alignment_pow;        /* power of 2 for calculating offsets */
  short    bytes_per;            /* 4 or 8 */
  char     dbpath[PATH_MAX+1];   /* full pathname to the database file */

  int      fd;

  rbtree*  offset_tree;
  rbtree*  record_tree;
} db_meta_t;

static db_meta_t* dbmeta_new_direct( const char* dbfilename )
{
  char hbuf[256];
  db_meta_t *dbmeta;

  dbmeta = (db_meta_t*)xcalloc( 1, sizeof( db_meta_t ));
  if(!dbmeta) {
    CfOut(OUTPUT_LEVEL_ERROR, "", "Error allocating memory : %s\n", strerror(errno));
    return NULL;
  }

  realpath( dbfilename, dbmeta->dbpath );
  if ( -1 == ( dbmeta->fd = open( dbmeta->dbpath, O_RDONLY) ) ) {
    CfOut(OUTPUT_LEVEL_ERROR, "", "Failure opening file [%s] : %s\n", dbmeta->dbpath, strerror( errno ));
    if(dbmeta) free(dbmeta);
    return NULL;
  }

  if ( 256 != read(dbmeta->fd, hbuf, 256 ) ) {
    CfOut(OUTPUT_LEVEL_ERROR, "", "Failure reading from database [%s] : %s\n", dbmeta->dbpath, strerror( errno ));
    close( dbmeta->fd );
    if(dbmeta) free(dbmeta);
    return NULL;
  }

  memcpy(&(dbmeta->bucket_count), hbuf + 40 , sizeof(uint64_t));
  dbmeta->bucket_offset = 256;
  uint8_t opts;
  memcpy(&opts, hbuf + 36, sizeof(uint8_t));
  dbmeta->bytes_per     = (opts & HDBTLARGE) ? sizeof(uint64_t) : sizeof(uint32_t);

  memcpy(&(dbmeta->record_count), hbuf + 48, sizeof(uint64_t));
  memcpy(&(dbmeta->record_offset), hbuf + 64, sizeof(uint64_t));
  memcpy(&(dbmeta->alignment_pow), hbuf + 34, sizeof(uint8_t));
  dbmeta->offset_tree   = NULL;
  dbmeta->record_tree   = NULL;

  CfOut(OUTPUT_LEVEL_VERBOSE, "", "Database            : %s\n",   dbmeta->dbpath );
  CfOut(OUTPUT_LEVEL_VERBOSE, "", "  number of buckets : %llu\n", (long long unsigned)dbmeta->bucket_count );
  CfOut(OUTPUT_LEVEL_VERBOSE, "", "  offset of buckets : %llu\n", (long long unsigned)dbmeta->bucket_offset );
  CfOut(OUTPUT_LEVEL_VERBOSE, "", "  bytes per pointer : %llu\n", (long long unsigned)dbmeta->bytes_per );
  CfOut(OUTPUT_LEVEL_VERBOSE, "", "  alignment power   : %llu\n", (long long unsigned)dbmeta->alignment_pow);
  CfOut(OUTPUT_LEVEL_VERBOSE, "", "  number of records : %llu\n", (long long unsigned)dbmeta->record_count );
  CfOut(OUTPUT_LEVEL_VERBOSE, "", "  offset of records : %llu\n", (long long unsigned)dbmeta->record_offset );

  return dbmeta;
}

static void dbmeta_free( db_meta_t* dbmeta )
{
  struct sglib_rbtree_iterator iter;
  rbtree *tree, *element;
  tree = dbmeta->offset_tree;
  element = sglib_rbtree_it_init( &iter, tree );

  while ( NULL != element ) {
    sglib_rbtree_delete( &tree, element );
    if(element) free( element );
    element = sglib_rbtree_it_next( &iter );
  }

  tree = dbmeta->record_tree;
  element = sglib_rbtree_it_init( &iter, tree );

  while ( NULL != element ) {
    sglib_rbtree_delete( &tree, element );
    if(element) free( element );
    element = sglib_rbtree_it_next( &iter );
  }

  close( dbmeta->fd );

  if(dbmeta) free( dbmeta );
}

static int add_offset_to_tree_unless_exists( rbtree** tree, uint64_t offset, int64_t bucket_index )
{
  rbtree *other = NULL;

  rbtree *new_node       = (rbtree*)xcalloc( 1, sizeof( rbtree ));
  if(!new_node) {
    CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to allocate tree node\n");
    return 1;
  }
  new_node->offset       = offset;
  new_node->bucket_index = bucket_index;

  other = sglib_rbtree_find_member( *tree, new_node );

  if ( NULL == other ) {
    sglib_rbtree_add( tree, new_node );
  } else {
    uint64_t diff = new_node->offset - other->offset;
    CfOut(OUTPUT_LEVEL_ERROR, "", "Duplicate offset for value %llu at index %lld, other value %llu, other index %lld, diff %llu\n", 
        (long long unsigned)new_node->offset, (long long)new_node->bucket_index,
        (long long unsigned)other->offset   , (long long)other->bucket_index, 
        (long long unsigned)diff);
    if(new_node) free( new_node );
  }
  return 0;
}

static int dbmeta_populate_offset_tree( db_meta_t* dbmeta )
{
  uint64_t i;

  if(lseek( dbmeta->fd, dbmeta->bucket_offset, SEEK_SET )==-1) {
    CfOut(OUTPUT_LEVEL_ERROR, "", "Error traversing bucket section to find record offsets : %s\n", strerror(errno));
    return 1;
  }

  for( i = 0 ; i < dbmeta->bucket_count ; i++ ) {
    uint64_t offset = 0LL;
    int           b = read( dbmeta->fd, &offset, dbmeta->bytes_per);

    if ( b != dbmeta->bytes_per ) {
      CfOut(OUTPUT_LEVEL_ERROR, "", "Read the wrong number of bytes (%d)\n", b );
      return 2;
    }

    /* if the value is > 0 then we have a number so do something with it */
    if ( offset > 0 ) {
      offset = offset << dbmeta->alignment_pow;
      if(add_offset_to_tree_unless_exists( &(dbmeta->offset_tree), offset, i )) {
        return 3;
      }
    }
 }

 CfOut(OUTPUT_LEVEL_VERBOSE, "", "Found %llu buckets with offsets\n", (long long unsigned)sglib_rbtree_len( dbmeta->offset_tree ));
 return 0;
}

enum {                                  // enumeration for magic data
 MAGIC_DATA_BLOCK = 0xc8,               // for data block
 MAGIC_FREE_BLOCK = 0xb0                // for free block
};

static int read_vary_int( int fd, uint32_t* result )
{
  uint64_t      num = 0;
  unsigned int base = 1;
  unsigned int    i = 0;
  int    read_bytes = 0;
  char c;

  while ( true ) {
    read_bytes += read( fd, &c, 1 );
    if ( c >= 0 ) {
      num += (c * base);
      break;
    }
    num += ( base * ( c + 1 ) * -1 );
    base <<= 7;
    i += 1;
  }

  *result = num;

  return read_bytes;
}


static bool dbmeta_read_one_rec( db_meta_t *dbmeta, tcrec* rec )
{
  if(lseek( dbmeta->fd, rec->offset, SEEK_SET )==-1) {
    CfOut(OUTPUT_LEVEL_ERROR, "", "Error traversing record section to find records : \n" );
  }

  while( true ) {
    // get the location of the current read
    //rec->offset = lseek64( dbmeta->fd, 0, SEEK_CUR );
    rec->offset = lseek( dbmeta->fd, 0, SEEK_CUR );
    if(rec->offset == (off_t) -1) {
      CfOut(OUTPUT_LEVEL_ERROR, "", "Error traversing record section to find records : \n" );
    }
    //rec->offset = ftell( dbmeta->fd );

    if ( 1 != read(dbmeta->fd, &(rec->magic), 1 ) ) {
      CfOut(OUTPUT_LEVEL_ERROR, "", "ERROR: Failure reading 1 byte, %s\n", strerror( errno ));
      return false;
    }

    if ( MAGIC_DATA_BLOCK ==  rec->magic ) {
      CfOut(OUTPUT_LEVEL_VERBOSE, "", "off=%llu[c8]\n", rec->offset);
      int length = 1;
  
      length += read( dbmeta->fd, &(rec->hash), 1 );
      length += read( dbmeta->fd, &(rec->left), dbmeta->bytes_per );
      rec->left = rec->left << dbmeta->alignment_pow;
  
      length += read( dbmeta->fd, &(rec->right), dbmeta->bytes_per );
      rec->right = rec->right << dbmeta->alignment_pow;
  
      length += read( dbmeta->fd, &(rec->pad_size), 2 );
      length += rec->pad_size;
     
      length += read_vary_int( dbmeta->fd, &(rec->key_size ));
      length += read_vary_int( dbmeta->fd, &(rec->rec_size ));
  
      rec->length = length + rec->key_size + rec->rec_size;
      return true;
  
    } else if ( MAGIC_FREE_BLOCK == rec->magic ) {
      CfOut(OUTPUT_LEVEL_VERBOSE, "", "off=%llu[b0]\n", rec->offset);
      uint32_t length;
      rec->length = 1;
      rec->length += read(dbmeta->fd, &length, sizeof( length ));
      rec->length += length;
      return true;
  
    } else {
      // read a non-magic byte, so skip it
      /*
      CfOut(OUTPUT_LEVEL_ERROR, "", "\nERROR : Read the start of a record at offset %llu, got %x instead of %x or %x\n",
            (long long unsigned)rec->offset, rec->magic, MAGIC_DATA_BLOCK, MAGIC_FREE_BLOCK );
      return false;
      */
    }
  }
  CfOut(OUTPUT_LEVEL_ERROR, "", "\nERROR : read loop reached here.\n");
  return false;
}

static int dbmeta_populate_record_tree( db_meta_t* dbmeta )
{
  off_t   offset;
  uint64_t data_blocks = 0;
  uint64_t free_blocks = 0;
  struct stat st;

  offset = dbmeta->record_offset;
  if(fstat( dbmeta->fd, &st ) == -1) {
    CfOut(OUTPUT_LEVEL_ERROR, "", "Error getting file stats :%s\n", strerror(errno));
    return 1;
  }

  while( offset < st.st_size ) {

    tcrec new_rec;
    memset(&new_rec, 0, sizeof(tcrec));
    new_rec.offset = offset;

    // read a variable-length record
    if( !dbmeta_read_one_rec( dbmeta, &new_rec )) { 
      CfOut(OUTPUT_LEVEL_ERROR, "", "Unable to fetch a new record from DB file\n");
      return 2;
    } else {
      offset = new_rec.offset + new_rec.length;
    }

    // if it is a data record then:
    // for the record, its left and right do:
    // look up that record in the offset tree
    // 1) remove it if it exists
    // 2) add it to the record_tree if it doesn't
 
    if ( MAGIC_DATA_BLOCK == new_rec.magic ) {

      if ( new_rec.offset > 0 ) {

        rbtree  find_me;
        rbtree *found;
        find_me.offset = new_rec.offset;
        
        if ( sglib_rbtree_delete_if_member( &(dbmeta->offset_tree), &find_me, &found ) != 0 ) { 
          if(found) free( found );
        } else {
          rbtree*  new_node = (rbtree*)xcalloc( 1, sizeof( rbtree ));
          if(!new_node) {
            CfOut(OUTPUT_LEVEL_ERROR, "", "Error allocating a tree node\n");
            return 3;
          }
          new_node->offset = new_rec.offset;
          sglib_rbtree_add(&(dbmeta->record_tree), new_node);
        }
      } else {
        CfOut(OUTPUT_LEVEL_ERROR, "", "How do you have a new_rec.offset that is <= 0 ???\n");
      }

      if ( new_rec.left > 0 ) {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ">>> handle left %llu\n", new_rec.left);
        if( add_offset_to_tree_unless_exists( &(dbmeta->offset_tree), new_rec.left, -1 )) {
          return 4;
        }
      }

      if ( new_rec.right > 0 ) {
        CfOut(OUTPUT_LEVEL_VERBOSE, "", ">>> handle right %llu\n", new_rec.right);
        if(add_offset_to_tree_unless_exists( &(dbmeta->offset_tree), new_rec.right, -1 )) {
          return 4;
        }
      }

      data_blocks++;
    } else if ( MAGIC_FREE_BLOCK == new_rec.magic ) {
      // if it is a fragment record, then skip it
      free_blocks++;
    } else {
      CfOut(OUTPUT_LEVEL_ERROR, "", "NO record found at offset %llu\n", (long long unsigned)new_rec.offset );
    }
  }

  // if we are not at the end of the file, output the current file offset
  // with an appropriate message and return
  CfOut(OUTPUT_LEVEL_VERBOSE, "",  "Found %llu data records and %llu free block records\n", data_blocks, free_blocks);

  return 0;
}

static int dbmeta_get_results( db_meta_t *dbmeta )
{
  uint64_t buckets_no_record = sglib_rbtree_len( dbmeta->offset_tree) ;
  uint64_t records_no_bucket = sglib_rbtree_len( dbmeta->record_tree) ;
  int ret = 0;

  CfOut(OUTPUT_LEVEL_VERBOSE, "", "Found %llu offsets listed in buckets that do not have records\n", buckets_no_record);
  CfOut(OUTPUT_LEVEL_VERBOSE, "", "Found %llu records in data that do not have an offset pointing to them\n", records_no_bucket);

  if ( buckets_no_record > 0 ) {
    ret += 1;
  }

  if ( records_no_bucket > 0 ) {
    ret += 2;
  }
  return ret;
}

int CheckTokyoDBCoherence( char *path )
{
  int ret = 0;
  db_meta_t *dbmeta;

  dbmeta = dbmeta_new_direct( path );
  if(dbmeta==NULL) {
    return 1;
  }

  CfOut(OUTPUT_LEVEL_VERBOSE, "", "Populating with bucket section offsets\n");
  ret = dbmeta_populate_offset_tree( dbmeta );
  if(ret) goto clean;

  CfOut(OUTPUT_LEVEL_VERBOSE, "", "Populating with record section offsets\n");
  ret = dbmeta_populate_record_tree( dbmeta );
  if(ret) goto clean;

  ret = dbmeta_get_results( dbmeta );

clean:
  if(dbmeta) dbmeta_free( dbmeta );

  return ret;
}
#else
int CheckTokyoDBCoherence( char *path )
{
  return 0;
}
#endif

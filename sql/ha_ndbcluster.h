/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  This file defines the NDB Cluster handler: the interface between MySQL and
  NDB Cluster
*/

/* The class defining a handle to an NDB Cluster table */

#ifdef __GNUC__
#pragma interface                       /* gcc class implementation */
#endif

#include <ndbapi_limits.h>
#include <ndb_types.h>

class Ndb;             // Forward declaration
class NdbOperation;    // Forward declaration
class NdbConnection;   // Forward declaration
class NdbRecAttr;      // Forward declaration
class NdbResultSet;    // Forward declaration

typedef enum ndb_index_type {
  UNDEFINED_INDEX = 0,
  PRIMARY_KEY_INDEX = 1,
  UNIQUE_INDEX = 2,
  ORDERED_INDEX = 3
} NDB_INDEX_TYPE;


typedef struct st_ndbcluster_share {
  THR_LOCK lock;
  pthread_mutex_t mutex;
  char *table_name;
  uint table_name_length,use_count;
} NDB_SHARE;

class ha_ndbcluster: public handler
{
 public:
  ha_ndbcluster(TABLE *table);
  ~ha_ndbcluster();

  int open(const char *name, int mode, uint test_if_locked);
  int close(void);

  int write_row(byte *buf);
  int update_row(const byte *old_data, byte *new_data);
  int delete_row(const byte *buf);
  int index_init(uint index);
  int index_end();
  int index_read(byte *buf, const byte *key, uint key_len, 
		 enum ha_rkey_function find_flag);
  int index_read_idx(byte *buf, uint index, const byte *key, uint key_len, 
		     enum ha_rkey_function find_flag);
  int index_next(byte *buf);
  int index_prev(byte *buf);
  int index_first(byte *buf);
  int index_last(byte *buf);
  int rnd_init(bool scan=1);
  int rnd_end();
  int rnd_next(byte *buf);
  int rnd_pos(byte *buf, byte *pos);
  void position(const byte *record);
  int read_range_first(const key_range *start_key,
		       const key_range *end_key,
		       bool sorted);
  int read_range_next(bool eq_range);


  void info(uint);
  int extra(enum ha_extra_function operation);
  int extra_opt(enum ha_extra_function operation, ulong cache_size);
  int reset();
  int external_lock(THD *thd, int lock_type);
  int start_stmt(THD *thd);
  const char * table_type() const { return("ndbcluster");}
  const char ** bas_ext() const;
  ulong table_flags(void) const { return m_table_flags; }
  ulong index_flags(uint idx) const;
  uint max_record_length() const { return NDB_MAX_TUPLE_SIZE; };
  uint max_keys()          const { return MAX_KEY;  }
  uint max_key_parts()     const { return NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY; };
  uint max_key_length()    const { return NDB_MAX_KEY_SIZE;};

  int rename_table(const char *from, const char *to);
  int delete_table(const char *name);
  int create(const char *name, TABLE *form, HA_CREATE_INFO *info);
  THR_LOCK_DATA **store_lock(THD *thd,
			     THR_LOCK_DATA **to,
			     enum thr_lock_type lock_type);

  bool low_byte_first() const
    { 
#ifdef WORDS_BIGENDIAN
      return false;
#else
      return true;
#endif
    }
  bool has_transactions()  { return true; }

  const char* index_type(uint key_number) {
    switch (get_index_type(key_number)) {
    case ORDERED_INDEX:
      return "BTREE";
    case UNIQUE_INDEX:
    case PRIMARY_KEY_INDEX:
    default:
      return "HASH";
    }
  }

  double scan_time();
  ha_rows records_in_range(int inx,
			   const byte *start_key,uint start_key_len,
			   enum ha_rkey_function start_search_flag,
			   const byte *end_key,uint end_key_len,
			   enum ha_rkey_function end_search_flag);

  void start_bulk_insert(ha_rows rows);
  int end_bulk_insert();

  static Ndb* seize_ndb();
  static void release_ndb(Ndb* ndb);
  uint8 table_cache_type() { return HA_CACHE_TBL_NOCACHE; }
    
 private:
  int alter_table_name(const char *from, const char *to);
  int drop_table();
  int create_index(const char *name, KEY *key_info, bool unique);
  int create_ordered_index(const char *name, KEY *key_info);
  int create_unique_index(const char *name, KEY *key_info);
  int initialize_autoincrement(const void* table);
  int get_metadata(const char* path);
  void release_metadata();
  const char* get_index_name(uint idx_no) const;
  const char* get_unique_index_name(uint idx_no) const;
  NDB_INDEX_TYPE get_index_type(uint idx_no) const;
  NDB_INDEX_TYPE get_index_type_from_table(uint index_no) const;
  
  int pk_read(const byte *key, uint key_len, 
	      byte *buf);
  int unique_index_read(const byte *key, uint key_len, 
			byte *buf);
  int ordered_index_scan(const key_range *start_key,
			 const key_range *end_key,
			 bool sorted, byte* buf);
  int full_table_scan(byte * buf);
  int next_result(byte *buf); 
#if 0
  int filtered_scan(const byte *key, uint key_len, 
		    byte *buf,
		    enum ha_rkey_function find_flag);
#endif

  void unpack_record(byte *buf);

  void set_dbname(const char *pathname);
  void set_tabname(const char *pathname);
  void set_tabname(const char *pathname, char *tabname);

  bool set_hidden_key(NdbOperation*,
		      uint fieldnr, const byte* field_ptr);
  int set_ndb_key(NdbOperation*, Field *field,
		  uint fieldnr, const byte* field_ptr);
  int set_ndb_value(NdbOperation*, Field *field, uint fieldnr);
  int get_ndb_value(NdbOperation*, uint fieldnr, byte *field_ptr);
  int set_primary_key(NdbOperation *op, const byte *key);
  int set_primary_key(NdbOperation *op);
  int set_bounds(NdbOperation *ndb_op, const key_range *key,
		 int bound);
  int key_cmp(uint keynr, const byte * old_row, const byte * new_row);
  void print_results();

  longlong get_auto_increment();

  int ndb_err(NdbConnection*);

 private:
  int check_ndb_connection();

  NdbConnection *m_active_trans;
  NdbResultSet *m_active_cursor;
  Ndb *m_ndb;
  void *m_table;		
  char m_dbname[FN_HEADLEN];
  //char m_schemaname[FN_HEADLEN];
  char m_tabname[FN_HEADLEN];
  ulong m_table_flags;
  THR_LOCK_DATA m_lock;
  NDB_SHARE *m_share;
  NDB_INDEX_TYPE  m_indextype[MAX_KEY];
  const char*  m_unique_index_name[MAX_KEY];
  NdbRecAttr *m_value[NDB_MAX_ATTRIBUTES_IN_TABLE];
  bool m_use_write;
  bool retrieve_all_fields;
  ha_rows rows_to_insert;
  ha_rows rows_inserted;
  ha_rows bulk_insert_rows;
};

bool ndbcluster_init(void);
bool ndbcluster_end(void);

int ndbcluster_commit(THD *thd, void* ndb_transaction);
int ndbcluster_rollback(THD *thd, void* ndb_transaction);

void ndbcluster_close_connection(THD *thd);

int ndbcluster_discover(const char* dbname, const char* name,
			const void** frmblob, uint* frmlen);
int ndbcluster_drop_database(const char* path);









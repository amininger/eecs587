#include "epmem_sql_containers.h"

//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////
// Statement Functions (epmem::statements)
//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////

epmem_master_statement_container::epmem_master_statement_container( agent *new_agent ): soar_module::sqlite_statement_container( new_agent->epmem_db )
{
	soar_module::sqlite_database *new_db = new_agent->epmem_db;

	// [TABLE] times: (id)
	add_structure( "CREATE TABLE IF NOT EXISTS times (id INTEGER PRIMARY KEY)" );

	// [TABLE] node_unique: (child_id, parent_id, attrib, value)
	add_structure( "CREATE TABLE IF NOT EXISTS node_unique (child_id INTEGER PRIMARY KEY AUTOINCREMENT,parent_id INTEGER,attrib INTEGER, value INTEGER)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS node_unique_parent_attrib_value ON node_unique (parent_id,attrib,value)" );

	// [TABLE] edge_unique: (parent_id, q0, w, q1, last)
	add_structure( "CREATE TABLE IF NOT EXISTS edge_unique (parent_id INTEGER PRIMARY KEY AUTOINCREMENT,q0 INTEGER,w INTEGER,q1 INTEGER, last INTEGER)" );
	add_structure( "CREATE INDEX IF NOT EXISTS edge_unique_q0_w_last ON edge_unique (q0,w,last)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS edge_unique_q0_w_q1 ON edge_unique (q0,w,q1)" );

	// [TABLE] lti: (parent_id, letter, num, time_id)
	add_structure( "CREATE TABLE IF NOT EXISTS lti (parent_id INTEGER PRIMARY KEY, letter INTEGER, num INTEGER, time_id INTEGER)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS lti_letter_num ON lti (letter,num)" );

	// [TABLE] temporal_symbol_hash: (id, sym_const, sym_type)
	add_structure( "CREATE TABLE IF NOT EXISTS temporal_symbol_hash (id INTEGER PRIMARY KEY, sym_const NONE, sym_type INTEGER)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS temporal_symbol_hash_const_type ON temporal_symbol_hash (sym_type,sym_const)" );

	// workaround for tree: type 1 = IDENTIFIER_SYMBOL_TYPE
	add_structure( "INSERT OR IGNORE INTO temporal_symbol_hash (id,sym_const,sym_type) VALUES (0,NULL,1)" );

	// workaround for acceptable preference wmes: id 1 = "operator+"
	add_structure( "INSERT OR IGNORE INTO temporal_symbol_hash (id,sym_const,sym_type) VALUES (1,'operator*',2)" );
	
	// [TABLE] vars: (id, value)
	add_structure( "CREATE TABLE IF NOT EXISTS vars (id INTEGER PRIMARY KEY,value NONE)" );

	// [TABLE] ascii: (ascii_num, ascii_char)
	// adding an ascii table just to make lti queries easier when inspecting database
	add_structure( "CREATE TABLE IF NOT EXISTS ascii (ascii_num INTEGER PRIMARY KEY, ascii_chr TEXT)" );
	add_structure( "DELETE FROM ascii" );
	{
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (65,'A')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (66,'B')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (67,'C')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (68,'D')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (69,'E')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (70,'F')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (71,'G')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (72,'H')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (73,'I')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (74,'J')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (75,'K')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (76,'L')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (77,'M')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (78,'N')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (79,'O')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (80,'P')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (81,'Q')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (82,'R')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (83,'S')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (84,'T')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (85,'U')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (86,'V')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (87,'W')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (88,'X')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (89,'Y')" );
		add_structure( "INSERT INTO ascii (ascii_num, ascii_chr) VALUES (90,'Z')" );
	}

	//

	add_time = new soar_module::sqlite_statement( new_db, "INSERT INTO times (id) VALUES (?)" );
	add( add_time );

	//

	add_node_unique = new soar_module::sqlite_statement( new_db, "INSERT INTO node_unique (parent_id,attrib,value) VALUES (?,?,?)" );
	add( add_node_unique );

	find_node_unique = new soar_module::sqlite_statement( new_db, "SELECT child_id FROM node_unique WHERE parent_id=? AND attrib=? AND value=?" );
	add( find_node_unique );

  // E587: AM: returns the row of the given edge looked up by id
	get_node_unique = new soar_module::sqlite_statement(new_db, "SELECT parent_id, attrib, value FROM node_unique WHERE child_id=?");
	add(get_node_unique);

	//

	add_edge_unique = new soar_module::sqlite_statement( new_db, "INSERT INTO edge_unique (q0,w,q1,last) VALUES (?,?,?,?)" );
	add( add_edge_unique );

	find_edge_unique = new soar_module::sqlite_statement( new_db, "SELECT parent_id, q1 FROM edge_unique WHERE q0=? AND w=?" );
	add( find_edge_unique );

	find_edge_unique_shared = new soar_module::sqlite_statement( new_db, "SELECT parent_id, q1 FROM edge_unique WHERE q0=? AND w=? AND q1=?" );
	add( find_edge_unique_shared );
	
  // E587: AM: returns the row of the given edge looked up by id
	get_edge_unique = new soar_module::sqlite_statement(new_db, "SELECT q0,w,q1,last FROM edge_unique WHERE parent_id=?");
	add(get_edge_unique);

	//

	get_node_desc = new soar_module::sqlite_statement(new_db, "SELECT f.child_id, f.parent_id, h1.sym_const, h2.sym_const, h1.sym_type, h2.sym_type FROM node_unique f, temporal_symbol_hash h1, temporal_symbol_hash h2 WHERE f.child_id=? AND f.attrib=h1.id AND f.value=h2.id");
	add(get_node_desc);

	get_edge_desc = new soar_module::sqlite_statement(new_db, "SELECT f.q0, h.sym_const, f.q1, h.sym_type, lti.letter, lti.num FROM edge_unique f INNER JOIN temporal_symbol_hash h ON f.w=h.id LEFT JOIN lti ON (f.q1=lti.parent_id AND lti.time_id <= ?) WHERE f.parent_id=?");
	add(get_edge_desc);

	//

	promote_id = new soar_module::sqlite_statement( new_db, "INSERT OR IGNORE INTO lti (parent_id,letter,num,time_id) VALUES (?,?,?,?)" );
	add( promote_id );

	find_lti = new soar_module::sqlite_statement( new_db, "SELECT parent_id FROM lti WHERE letter=? AND num=?" );
	add( find_lti );

	find_lti_promotion_time = new soar_module::sqlite_statement( new_db, "SELECT time_id FROM lti WHERE parent_id=?" );
	add( find_lti_promotion_time );

	//

	hash_get = new soar_module::sqlite_statement( new_db, "SELECT id FROM temporal_symbol_hash WHERE sym_type=? AND sym_const=?" );
	add( hash_get );

	hash_add = new soar_module::sqlite_statement( new_db, "INSERT INTO temporal_symbol_hash (sym_type,sym_const) VALUES (?,?)" );
	add( hash_add );


	var_get = new soar_module::sqlite_statement( new_db, "SELECT value FROM vars WHERE id=?" );
	add( var_get );

	var_set = new soar_module::sqlite_statement( new_db, "REPLACE INTO vars (id,value) VALUES (?,?)" );
	add( var_set );


	begin = new soar_module::sqlite_statement( new_db, "BEGIN" );
	add( begin );

	commit = new soar_module::sqlite_statement( new_db, "COMMIT" );
	add( commit );

	rollback = new soar_module::sqlite_statement( new_db, "ROLLBACK" );
	add( rollback );


	valid_episode = new soar_module::sqlite_statement( new_db, "SELECT COUNT(*) AS ct FROM times WHERE id=?" );
	add( valid_episode );

	next_episode = new soar_module::sqlite_statement( new_db, "SELECT id FROM times WHERE id>? ORDER BY id ASC LIMIT 1" );
	add( next_episode );

	prev_episode = new soar_module::sqlite_statement( new_db, "SELECT id FROM times WHERE id<? ORDER BY id DESC LIMIT 1" );
	add( prev_episode );

}



// E587: AM: moving onto worker processor
epmem_common_statement_container::epmem_common_statement_container( soar_module::sqlite_database *new_db ): soar_module::sqlite_statement_container( new_db )
{


	//

	//add_structure( "CREATE TABLE IF NOT EXISTS temporal_symbol_hash (id INTEGER PRIMARY KEY, sym_const NONE, sym_type INTEGER)" );
	//add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS temporal_symbol_hash_const_type ON temporal_symbol_hash (sym_type,sym_const)" );

	//// workaround for tree: type 1 = IDENTIFIER_SYMBOL_TYPE
	//add_structure( "INSERT OR IGNORE INTO temporal_symbol_hash (id,sym_const,sym_type) VALUES (0,NULL,1)" );

	//// workaround for acceptable preference wmes: id 1 = "operator+"
	//add_structure( "INSERT OR IGNORE INTO temporal_symbol_hash (id,sym_const,sym_type) VALUES (1,'operator*',2)" );
	
	// [TABLE] vars: (id, value)
	add_structure( "CREATE TABLE IF NOT EXISTS vars (id INTEGER PRIMARY KEY,value NONE)" );

	// [TABLE] rit_left_nodes: (min, max)
	add_structure( "CREATE TABLE IF NOT EXISTS rit_left_nodes (min INTEGER, max INTEGER)" );

	// [TABLE] rit_right_nodes: (node)
	add_structure( "CREATE TABLE IF NOT EXISTS rit_right_nodes (node INTEGER)" );

	//
	// E587: AM:
	var_get = new soar_module::sqlite_statement( new_db, "SELECT value FROM vars WHERE id=?" );
	add( var_get );

	var_set = new soar_module::sqlite_statement( new_db, "REPLACE INTO vars (id,value) VALUES (?,?)" );
	add( var_set );

	//

	rit_add_left = new soar_module::sqlite_statement( new_db, "INSERT INTO rit_left_nodes (min,max) VALUES (?,?)" );
	add( rit_add_left );

	rit_truncate_left = new soar_module::sqlite_statement( new_db, "DELETE FROM rit_left_nodes" );
	add( rit_truncate_left );

	rit_add_right = new soar_module::sqlite_statement( new_db, "INSERT INTO rit_right_nodes (node) VALUES (?)" );
	add( rit_add_right );

	rit_truncate_right = new soar_module::sqlite_statement( new_db, "DELETE FROM rit_right_nodes" );
	add( rit_truncate_right );

	//
//	// E587: AM:
	//hash_get = new soar_module::sqlite_statement( new_db, "SELECT id FROM temporal_symbol_hash WHERE sym_type=? AND sym_const=?" );
	//add( hash_get );

	//hash_add = new soar_module::sqlite_statement( new_db, "INSERT INTO temporal_symbol_hash (sym_type,sym_const) VALUES (?,?)" );
	//add( hash_add );
}

epmem_graph_statement_container::epmem_graph_statement_container( soar_module::sqlite_database* new_db ): soar_module::sqlite_statement_container( new_db )
{

	//

	add_structure( "CREATE TABLE IF NOT EXISTS times (id INTEGER PRIMARY KEY)" );

	add_structure( "CREATE TABLE IF NOT EXISTS node_now (id INTEGER,start INTEGER)" );
	add_structure( "CREATE INDEX IF NOT EXISTS node_now_start ON node_now (start)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS node_now_id_start ON node_now (id,start DESC)" );

	add_structure( "CREATE TABLE IF NOT EXISTS edge_now (id INTEGER,start INTEGER)" );
	add_structure( "CREATE INDEX IF NOT EXISTS edge_now_start ON edge_now (start)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS edge_now_id_start ON edge_now (id,start DESC)" );

	add_structure( "CREATE TABLE IF NOT EXISTS node_point (id INTEGER,start INTEGER)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS node_point_id_start ON node_point (id,start DESC)" );
	add_structure( "CREATE INDEX IF NOT EXISTS node_point_start ON node_point (start)" );

	add_structure( "CREATE TABLE IF NOT EXISTS edge_point (id INTEGER,start INTEGER)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS edge_point_id_start ON edge_point (id,start DESC)" );
	add_structure( "CREATE INDEX IF NOT EXISTS edge_point_start ON edge_point (start)" );

	add_structure( "CREATE TABLE IF NOT EXISTS node_range (rit_node INTEGER,start INTEGER,end INTEGER,id INTEGER)" );
	add_structure( "CREATE INDEX IF NOT EXISTS node_range_lower ON node_range (rit_node,start)" );
	add_structure( "CREATE INDEX IF NOT EXISTS node_range_upper ON node_range (rit_node,end)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS node_range_id_start ON node_range (id,start DESC)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS node_range_id_end_start ON node_range (id,end DESC,start)" );

	add_structure( "CREATE TABLE IF NOT EXISTS edge_range (rit_node INTEGER,start INTEGER,end INTEGER,id INTEGER)" );
	add_structure( "CREATE INDEX IF NOT EXISTS edge_range_lower ON edge_range (rit_node,start)" );
	add_structure( "CREATE INDEX IF NOT EXISTS edge_range_upper ON edge_range (rit_node,end)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS edge_range_id_start ON edge_range (id,start DESC)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS edge_range_id_end_start ON edge_range (id,end DESC,start)" );

	add_structure( "CREATE TABLE IF NOT EXISTS node_unique (child_id INTEGER PRIMARY KEY AUTOINCREMENT,parent_id INTEGER,attrib INTEGER, value INTEGER)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS node_unique_parent_attrib_value ON node_unique (parent_id,attrib,value)" );

	add_structure( "CREATE TABLE IF NOT EXISTS edge_unique (parent_id INTEGER PRIMARY KEY AUTOINCREMENT,q0 INTEGER,w INTEGER,q1 INTEGER, last INTEGER)" );
	add_structure( "CREATE INDEX IF NOT EXISTS edge_unique_q0_w_last ON edge_unique (q0,w,last)" );
	add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS edge_unique_q0_w_q1 ON edge_unique (q0,w,q1)" );

	//

	// [TABLE] lti: (parent_id, letter, num, time_id)
	//add_structure( "CREATE TABLE IF NOT EXISTS lti (parent_id INTEGER PRIMARY KEY, letter INTEGER, num INTEGER, time_id INTEGER)" );
	//add_structure( "CREATE UNIQUE INDEX IF NOT EXISTS lti_letter_num ON lti (letter,num)" );

	add_time = new soar_module::sqlite_statement( new_db, "INSERT INTO times (id) VALUES (?)" );
	add( add_time );

	//
	//promote_id = new soar_module::sqlite_statement( new_db, "INSERT OR IGNORE INTO lti (parent_id,letter,num,time_id) VALUES (?,?,?,?)" );
	//add( promote_id );

	//find_lti = new soar_module::sqlite_statement( new_db, "SELECT parent_id FROM lti WHERE letter=? AND num=?" );
	//add( find_lti );

	//find_lti_promotion_time = new soar_module::sqlite_statement( new_db, "SELECT time_id FROM lti WHERE parent_id=?" );
	//add( find_lti_promotion_time );


	//

	add_node_now = new soar_module::sqlite_statement( new_db, "INSERT INTO node_now (id,start) VALUES (?,?)" );
	add( add_node_now );

	delete_node_now = new soar_module::sqlite_statement( new_db, "DELETE FROM node_now WHERE id=?" );
	add( delete_node_now );

	add_node_point = new soar_module::sqlite_statement( new_db, "INSERT INTO node_point (id,start) VALUES (?,?)" );
	add( add_node_point );

	add_node_range = new soar_module::sqlite_statement( new_db, "INSERT INTO node_range (rit_node,start,end,id) VALUES (?,?,?,?)" );
	add( add_node_range );


	add_node_unique = new soar_module::sqlite_statement( new_db, "INSERT INTO node_unique (parent_id,attrib,value) VALUES (?,?,?)" );
	add( add_node_unique );
	
  // E587: AM: inserts a node_unique but with a specified id
	add_node_unique_with_id = new soar_module::sqlite_statement( new_db, "INSERT INTO node_unique (child_id,parent_id,attrib,value) VALUES (?,?,?,?)");
	add( add_node_unique_with_id);

	find_node_unique = new soar_module::sqlite_statement( new_db, "SELECT child_id FROM node_unique WHERE parent_id=? AND attrib=? AND value=?" );
	add( find_node_unique );

  // E587: AM: returns the row of the given node looked up by id
	get_node_unique = new soar_module::sqlite_statement(new_db, "SELECT parent_id, attrib, value FROM node_unique WHERE child_id=?");
	add(get_node_unique);

	//

	add_edge_now = new soar_module::sqlite_statement( new_db, "INSERT INTO edge_now (id,start) VALUES (?,?)" );
	add( add_edge_now );

	delete_edge_now = new soar_module::sqlite_statement( new_db, "DELETE FROM edge_now WHERE id=?" );
	add( delete_edge_now );

	add_edge_point = new soar_module::sqlite_statement( new_db, "INSERT INTO edge_point (id,start) VALUES (?,?)" );
	add( add_edge_point );

	add_edge_range = new soar_module::sqlite_statement( new_db, "INSERT INTO edge_range (rit_node,start,end,id) VALUES (?,?,?,?)" );
	add( add_edge_range );


	add_edge_unique = new soar_module::sqlite_statement( new_db, "INSERT INTO edge_unique (q0,w,q1,last) VALUES (?,?,?,?)" );
	add( add_edge_unique );
	
  // E587: AM: inserts an edge_unique but with a specified id
	add_edge_unique_with_id = new soar_module::sqlite_statement(new_db, "INSERT INTO edge_unique (parent_id,q0,w,q1,last) VALUES (?,?,?,?,?)");
	add(add_edge_unique_with_id);

	find_edge_unique = new soar_module::sqlite_statement( new_db, "SELECT parent_id, q1 FROM edge_unique WHERE q0=? AND w=?" );
	add( find_edge_unique );

	find_edge_unique_shared = new soar_module::sqlite_statement( new_db, "SELECT parent_id, q1 FROM edge_unique WHERE q0=? AND w=? AND q1=?" );
	add( find_edge_unique_shared );

  // E587: AM: returns the row of the given edge looked up by id
	get_edge_unique = new soar_module::sqlite_statement(new_db, "SELECT q0,w,q1,last FROM edge_unique WHERE parent_id=?");
	add(get_edge_unique);

	

	//
	get_node_ids = new soar_module::sqlite_statement(new_db, "SELECT f.child_id FROM node_unique f WHERE f.child_id IN (SELECT n.id FROM node_now n WHERE n.start<= ? UNION ALL SELECT p.id FROM node_point p WHERE p.start=? UNION ALL SELECT e1.id FROM node_range e1, rit_left_nodes lt WHERE e1.rit_node=lt.min AND e1.end >= ? UNION ALL SELECT e2.id FROM node_range e2, rit_right_nodes rt WHERE e2.rit_node = rt.node AND e2.start <= ?) ORDER BY f.child_id ASC");
	add(get_node_ids);

	get_edge_ids = new soar_module::sqlite_statement(new_db, "SELECT f.parent_id FROM edge_unique f WHERE f.parent_id IN (SELECT n.id FROM edge_now n WHERE n.start<= ? UNION ALL SELECT p.id FROM edge_point p WHERE p.start = ? UNION ALL SELECT e1.id FROM edge_range e1, rit_left_nodes lt WHERE e1.rit_node=lt.min AND e1.end >= ? UNION ALL SELECT e2.id FROM edge_range e2, rit_right_nodes rt WHERE e2.rit_node = rt.node AND e2.start <= ?) ORDER BY f.parent_id ASC");
	add(get_edge_ids);

	//

	update_edge_unique_last = new soar_module::sqlite_statement( new_db, "UPDATE edge_unique SET last=? WHERE parent_id=?" );
	add( update_edge_unique_last );

	//

	// init statement pools
	{
		int j, k, m;

		const char* epmem_find_edge_queries[2][2] = {
			{
				"SELECT child_id, value, ? FROM node_unique WHERE parent_id=? AND attrib=?",
				"SELECT child_id, value, ? FROM node_unique WHERE parent_id=? AND attrib=? AND value=?"
			},
			{
				"SELECT parent_id, q1, last FROM edge_unique WHERE q0=? AND w=? AND ?<last ORDER BY last DESC",
				"SELECT parent_id, q1, last FROM edge_unique WHERE q0=? AND w=? AND q1=? AND ?<last"
			}
		};

		for ( j=EPMEM_RIT_STATE_NODE; j<=EPMEM_RIT_STATE_EDGE; j++ )
		{
			for ( k=0; k<=1; k++ )
			{
				pool_find_edge_queries[ j ][ k ] = new soar_module::sqlite_statement_pool( new_db, epmem_find_edge_queries[ j ][ k ] );
			}
		}

		//

		// Because the DB records when things are /inserted/, we need to offset
		// the start by 1 to /remove/ them at the right time. Ditto to even
		// include those intervals correctly
		const char *epmem_find_interval_queries[2][2][3] =
		{
			{
				{
					"SELECT (e.start - 1) AS start FROM node_range e WHERE e.id=? AND e.start<=? ORDER BY e.start DESC",
					"SELECT (e.start - 1) AS start FROM node_now e WHERE e.id=? AND e.start<=? ORDER BY e.start DESC",
					"SELECT (e.start - 1) AS start FROM node_point e WHERE e.id=? AND e.start<=? ORDER BY e.start DESC"
				},
				{
					"SELECT e.end AS end FROM node_range e WHERE e.id=? AND e.end>0 AND e.start<=? ORDER BY e.end DESC",
					"SELECT ? AS end FROM node_now e WHERE e.id=? AND e.start<=? ORDER BY e.start DESC",
					"SELECT e.start AS end FROM node_point e WHERE e.id=? AND e.start<=? ORDER BY e.start DESC"
				}
			},
			{
				{
					"SELECT (e.start - 1) AS start FROM edge_range e WHERE e.id=? AND e.start<=? ORDER BY e.start DESC",
					"SELECT (e.start - 1) AS start FROM edge_now e WHERE e.id=? AND e.start<=? ORDER BY e.start DESC",
					"SELECT (e.start - 1) AS start FROM edge_point e WHERE e.id=? AND e.start<=? ORDER BY e.start DESC"
				},
				{
					"SELECT e.end AS end FROM edge_range e WHERE e.id=? AND e.end>0 AND e.start<=? ORDER BY e.end DESC",
					"SELECT ? AS end FROM edge_now e WHERE e.id=? AND e.start<=? ORDER BY e.start DESC",
					"SELECT e.start AS end FROM edge_point e WHERE e.id=? AND e.start<=? ORDER BY e.start DESC"
				}
			},
		};

		for ( j=EPMEM_RIT_STATE_NODE; j<=EPMEM_RIT_STATE_EDGE; j++ )
		{
			for ( k=EPMEM_RANGE_START; k<=EPMEM_RANGE_END; k++ )
			{
				for( m=EPMEM_RANGE_EP; m<=EPMEM_RANGE_POINT; m++ )
				{
					pool_find_interval_queries[ j ][ k ][ m ] = new soar_module::sqlite_statement_pool( new_db, epmem_find_interval_queries[ j ][ k ][ m ] );
				}
			}
		}

		//

		// notice that the start and end queries in epmem_find_lti_queries are _asymetric_
		// in that the the starts have ?<e.start and the ends have ?<=e.start
		// this small difference means that the start of the very first interval
		// (ie. the one where the start is at or before the promotion time) will be ignored
		// then we can simply add a single epmem_interval to the queue, and it will
		// terminate any LTI interval appropriately
		const char *epmem_find_lti_queries[2][3] =
		{
			{
				"SELECT (e.start - 1) AS start FROM edge_range e WHERE e.id=? AND ?<e.start AND e.start<=? ORDER BY e.start DESC",
				"SELECT (e.start - 1) AS start FROM edge_now e WHERE e.id=? AND ?<e.start AND e.start<=? ORDER BY e.start DESC",
				"SELECT (e.start - 1) AS start FROM edge_point e WHERE e.id=? AND ?<e.start AND e.start<=? ORDER BY e.start DESC"
			},
			{
				"SELECT e.end AS end FROM edge_range e WHERE e.id=? AND e.end>0 AND ?<=e.start AND e.start<=? ORDER BY e.end DESC",
				"SELECT ? AS end FROM edge_now e WHERE e.id=? AND ?<=e.start AND e.start<=? ORDER BY e.start",
				"SELECT e.start AS end FROM edge_point e WHERE e.id=? AND ?<=e.start AND e.start<=? ORDER BY e.start DESC"
			}
		};

		for ( k=EPMEM_RANGE_START; k<=EPMEM_RANGE_END; k++ )
		{
			for( m=EPMEM_RANGE_EP; m<=EPMEM_RANGE_POINT; m++ )
			{
				pool_find_lti_queries[ k ][ m ] = new soar_module::sqlite_statement_pool( new_db, epmem_find_lti_queries[ k ][ m ] );
			}
		}

		//

		pool_dummy = new soar_module::sqlite_statement_pool( new_db, "SELECT ? as start" );
	}
}

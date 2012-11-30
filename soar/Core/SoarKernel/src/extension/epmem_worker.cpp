#include "epmem_worker.h"


epmem_worker::epmem_worker(){
	 epmem_db = new soar_module::sqlite_database();
	 epmem_stmts_common = NIL;
	 epmem_stmts_graph = NIL;
}

void epmem_worker::initialize(epmem_param_container* epmem_params, agent* my_agent){
	{
		// E587: AM:
		if ( epmem_db->get_status() != soar_module::disconnected )
		{
			return;
		}


		const char *db_path = ":memory:";

		// attempt connection
		epmem_db->connect( db_path );

		if ( epmem_db->get_status() == soar_module::problem )
		{
			char buf[256];
			//SNPRINTF( buf, 254, "DB ERROR: %s", epmem_db->get_errmsg() );
		}
		else
		{
			epmem_time_id time_max;
			soar_module::sqlite_statement *temp_q = NULL;
			soar_module::sqlite_statement *temp_q2 = NULL;

			// apply performance options
			{
				// page_size
				{
					switch ( epmem_params->page_size->get_value() )
					{
						case ( epmem_param_container::page_1k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 1024" );
							break;

						case ( epmem_param_container::page_2k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 2048" );
							break;

						case ( epmem_param_container::page_4k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 4096" );
							break;

						case ( epmem_param_container::page_8k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 8192" );
							break;

						case ( epmem_param_container::page_16k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 16384" );
							break;

						case ( epmem_param_container::page_32k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 32768" );
							break;

						case ( epmem_param_container::page_64k ):
							temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA page_size = 65536" );
							break;
					}

					temp_q->prepare();
					temp_q->execute();
					delete temp_q;
					temp_q = NULL;
				}

				// cache_size
				{
					std::string cache_sql( "PRAGMA cache_size = " );
					char* str = epmem_params->cache_size->get_string();
					cache_sql.append( str );
					free(str);
					str = NULL;

					temp_q = new soar_module::sqlite_statement( epmem_db, cache_sql.c_str() );

					temp_q->prepare();
					temp_q->execute();
					delete temp_q;
					temp_q = NULL;
				}

				// optimization
				if ( epmem_params->opt->get_value() == epmem_param_container::opt_speed )
				{
					// synchronous - don't wait for writes to complete (can corrupt the db in case unexpected crash during transaction)
					temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA synchronous = OFF" );
					temp_q->prepare();
					temp_q->execute();
					delete temp_q;
					temp_q = NULL;

					// journal_mode - no atomic transactions (can result in database corruption if crash during transaction)
					temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA journal_mode = OFF" );
					temp_q->prepare();
					temp_q->execute();
					delete temp_q;
					temp_q = NULL;

					// locking_mode - no one else can view the database after our first write
					temp_q = new soar_module::sqlite_statement( epmem_db, "PRAGMA locking_mode = EXCLUSIVE" );
					temp_q->prepare();
					temp_q->execute();
					delete temp_q;
					temp_q = NULL;
				}
			}

			// setup common structures/queries
			epmem_stmts_common = new epmem_common_statement_container( my_agent );
			epmem_stmts_common->structure();
			epmem_stmts_common->prepare();

			// setup graph structures/queries
			epmem_stmts_graph = new epmem_graph_statement_container( my_agent );
			epmem_stmts_graph->structure();
			epmem_stmts_graph->prepare();
		}
	}
}

void epmem_worker::add_new_episode(new_episode* episode){

}

new_episode* epmem_worker::remove_oldest_episode(){
	return new new_episode();
}

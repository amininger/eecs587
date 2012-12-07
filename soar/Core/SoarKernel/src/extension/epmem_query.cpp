#include "epmem_query.h"
#include "wma.h"
#include "soar_module.h"


epmem_cue_wme_struct::epmem_cue_wme_struct(epmem_packed_cue_wme* packed_wme, epmem_query* query){
    activation = packed_wme->activation;
    id = &query->symbols[packed_wme->id_index];
    attr = &query->symbols[packed_wme->attr_index];
    value = &query->symbols[packed_wme->value_index];
} 

epmem_packed_cue_wme_struct::epmem_packed_cue_wme_struct(wme* w, agent* my_agent, epmem_cue_symbol_table& sym_table){
    activation = wma_get_wme_activation(my_agent, w, true);
    id_index = sym_table.get_cue_symbol(w->id, my_agent);
    attr_index = sym_table.get_cue_symbol(w->attr, my_agent);
    value_index = sym_table.get_cue_symbol(w->value, my_agent);
}

int epmem_cue_symbol_table_struct::get_cue_symbol(Symbol* sym, agent* my_agent){
    epmem_cue_symbol_table_index::iterator i = sym_index.find(sym);
    if(i == sym_index.end()){
        int index = symbols->size();
        symbols->push_back(epmem_cue_symbol(sym, my_agent));
        sym_index.insert(std::pair<Symbol*, int>(sym, index));
        return index;
    } else {
        return i->second;
    }
}

epmem_cue_symbol_struct::epmem_cue_symbol_struct(Symbol* sym, agent* my_agent){
    if(sym->common.symbol_type != IDENTIFIER_SYMBOL_TYPE){
        is_id = false;
        is_lti = false;
        id = epmem_temporal_hash(my_agent, sym);
    } else if(sym->id.smem_lti){
        is_id = true;
        is_lti = true;
		my_agent->epmem_stmts_master->find_lti->bind_int(1, static_cast<uint64_t>(sym->id.name_letter));
		my_agent->epmem_stmts_master->find_lti->bind_int(2, static_cast<uint64_t>(sym->id.name_number));
		if (my_agent->epmem_stmts_master->find_lti->execute() == soar_module::row) {
            id = my_agent->epmem_stmts_master->find_lti->column_int(0);
            promotion_time = my_agent->epmem_stmts_master->find_lti->column_int(1);
			my_agent->epmem_stmts_master->find_lti->reinitialize();
		} else {
			my_agent->epmem_stmts_master->find_lti->reinitialize();
            id = EPMEM_NODEID_BAD;
		}
    } else {
        is_id = true;
        is_lti = false;
        id = sym->id.epmem_id;
    }
}
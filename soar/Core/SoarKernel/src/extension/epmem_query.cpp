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

// Utility class which can be used to pack/unpack messages using the data buffer provided
// Works by sequentially reading/writing along the buffer 
struct epmem_msg_packer{
    epmem_msg_packer(epmem_msg* new_msg):msg(new_msg){
        addr = &new_msg->data;
    }

    // Returns a value copy of what's next in the message buffer
    template <class T>
    T unpack(){
        T* ret_addr = (T*)addr;
        addr += sizeof(T);
        return *ret_addr;
    }

    // Copies the given value to the message bufffer
    template <class T>
    void pack(T& val){
        *((T*)addr) = val;
        addr += sizeof(T);
    }
    
    // Copies a whole vector to the message buffer (by-value copy)
    template <class T>
    void pack_vector(vector<T>& v){
        int size = v.size();
        pack(size);
        for(int i = 0; i < size; i++){
            pack(v[i]);
        }
    }

    // Reads a vector from the buffer (fills in the one given)
    template <class T>
    void unpack_vector(vector<T>& v){
        int size = unpack<int>();
        for(int i = 0; i < size; i++){
            v.push_back(unpack<T>());
        }
    }

    char* addr;
    epmem_msg* msg;
};



epmem_msg* epmem_query::pack(){
    int buffer_size = 
        sizeof(epmem_msg) +                                                     // epmem_msg
        sizeof(epmem_time_id) * 2 + sizeof(bool) +                              // before/after/do_graph_match
        sizeof(epmem_param_container::gm_ordering_choices) +                    // gm_order
        sizeof(int) * 3 +                                                       // pos_query_index, neg_query_index, level
        sizeof(int) + sizeof(epmem_time_id) * prohibits.size() +                // prohibits
        sizeof(int) + sizeof(int) * currents.size() +                           // currents
        sizeof(int) + sizeof(epmem_packed_cue_wme) * wmes.size() +              // wmes
        sizeof(int) + sizeof(epmem_cue_symbol) * symbols.size();                // symbols

    epmem_msg* msg = reinterpret_cast<epmem_msg*>(malloc(buffer_size));
    msg->type = SEARCH;
    msg->size = buffer_size;

    epmem_msg_packer packer(msg);
    
    // simple datastructures
    packer.pack<epmem_time_id>(before);
    packer.pack<epmem_time_id>(after);
    packer.pack<bool>(do_graph_match);
    packer.pack<epmem_param_container::gm_ordering_choices>(gm_order);
    packer.pack<int>(pos_query_index);
    packer.pack<int>(neg_query_index);
    packer.pack<int>(level);

    // vectors
    packer.pack_vector<epmem_time_id>(prohibits);
    packer.pack_vector<int>(currents);
    packer.pack_vector<epmem_packed_cue_wme>(wmes);
    packer.pack_vector<epmem_cue_symbol>(symbols);

    return msg;
}

void epmem_query::unpack(epmem_msg* msg){
    epmem_msg_packer unpacker(msg);
    
    // simple datastructures
    before = unpacker.unpack<epmem_time_id>();
    after = unpacker.unpack<epmem_time_id>();
    do_graph_match = unpacker.unpack<bool>();
    gm_order = unpacker.unpack<epmem_param_container::gm_ordering_choices>();
    pos_query_index = unpacker.unpack<int>();
    neg_query_index = unpacker.unpack<int>();
    level = unpacker.unpack<int>();

    // vectors
    unpacker.unpack_vector<epmem_time_id>(prohibits);
    unpacker.unpack_vector<int>(currents);
    unpacker.unpack_vector<epmem_packed_cue_wme>(wmes);
    unpacker.unpack_vector<epmem_cue_symbol>(symbols);
}

epmem_msg* query_rsp_data::pack(){
    int buffer_size =
        sizeof(epmem_msg) +         // epmem_msg
        sizeof(epmem_time_id) +     // best_episode
        2 * sizeof(double) +        // best_score and perfect_score
        sizeof(bool) +              // best_graph_matched
        sizeof(long) +              // best_cardinality
        sizeof(int);                // leaf_literals_size

    epmem_msg* msg = (epmem_msg*)malloc(buffer_size);
    msg->type = SEARCH_RESULT;
    msg->size = buffer_size;

    epmem_msg_packer packer(msg);

    packer.pack<epmem_time_id>(best_episode);
    packer.pack<double>(best_score);
    packer.pack<double>(perfect_score);
    packer.pack<bool>(best_graph_matched);
    packer.pack<long>(best_cardinality);
    packer.pack<int>(leaf_literals_size);

    return NIL;
}
void query_rsp_data::unpack(epmem_msg* msg){
    epmem_msg_packer unpacker(msg);

    best_episode = unpacker.unpack<double>();
    best_score = unpacker.pack<double>();
    perfect_score = unpacker.pack<double>();
    best_graph_matched = unpacker.pack<bool>();
    best_cardinality = unpacker.pack<long>();
    leaf_literals_size = unpacker.pack<int>();
}
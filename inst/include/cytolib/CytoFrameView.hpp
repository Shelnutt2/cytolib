/* Copyright 2019 Fred Hutchinson Cancer Research Center
 * See the included LICENSE file for details
 * on the licence that is granted to the user of this software.
 * CytoFrameView.hpp
 *
 *  Created on: Apr 3, 2018
 *      Author: wjiang2
 */

#ifndef INST_INCLUDE_CYTOLIB_CYTOFRAMEVIEW_HPP_
#define INST_INCLUDE_CYTOLIB_CYTOFRAMEVIEW_HPP_
#include "CytoFrame.hpp"
namespace cytolib
{
class CytoFrameView{
	CytoFramePtr ptr_;
	arma::uvec row_idx_;
	arma::uvec col_idx_;
	bool is_row_indexed_ = false;
	bool is_col_indexed_ = false;
public:
	CytoFrameView(){};
	CytoFrameView(CytoFramePtr ptr):ptr_(ptr){};
	CytoFramePtr get_cytoframe_ptr() const;
	bool is_row_indexed() const{return is_row_indexed_;};
	bool is_col_indexed() const{return is_col_indexed_;};
	bool is_empty() const{return (is_row_indexed_ && row_idx_.is_empty()) || (is_col_indexed_ && col_idx_.is_empty());};

	/*forwarded apis
	 *
	 */
	void scale_time_channel(string time_channel = "time"){
		get_cytoframe_ptr()->scale_time_channel(time_channel);
			}
	void set_readonly(bool flag){
		get_cytoframe_ptr()->set_readonly(flag);
	}
	bool get_readonly(){
			return get_cytoframe_ptr()->get_readonly();
		}
	virtual void flush_meta(){
		get_cytoframe_ptr()->flush_meta();
	};
	virtual void load_meta(){
		get_cytoframe_ptr()->load_meta();
	};
	string get_h5_file_path() const{
			return get_cytoframe_ptr()->get_h5_file_path();
		}
	vector<string> get_channels() const;
	vector<string> get_markers() const;
	void set_channels(const CHANNEL_MAP & chnl_map){get_cytoframe_ptr()->set_channels(chnl_map);}
	void convertToPb(pb::CytoFrame & fr_pb, const string & h5_filename, H5Option h5_opt) const{
		if(is_row_indexed_ || is_col_indexed_)
		{
			if(h5_opt != H5Option::copy)
				throw(domain_error("Only 'copy' H5Option is supported for the indexed CytoFrameView object!"));
			copy_realized(h5_filename);
			h5_opt = H5Option::skip;
		}

		get_cytoframe_ptr()->convertToPb(fr_pb, h5_filename, h5_opt);
	};
	void set_channel(const string & oldname, const string &newname)
	{
		get_cytoframe_ptr()->set_channel(oldname, newname);
	}
	string get_marker(const string & channel)
	{
		return get_cytoframe_ptr()->get_marker(channel);
	}

	void set_marker(const string & channelname, const string & markername)
	{
		get_cytoframe_ptr()->set_marker(channelname, markername);
	}
	void compensate(const compensation & comp)
	{
			get_cytoframe_ptr()->compensate(comp);
	}
	compensation get_compensation(const string & key = "SPILL")
	{
		return	get_cytoframe_ptr()->get_compensation(key);
	}
	void write_h5(const string & filename) const
	{
		get_cytoframe_ptr()->write_h5(filename);
	}

	KEY_WORDS get_keywords() const{
		KEY_WORDS res = get_cytoframe_ptr()->get_keywords();
		//TODO: we currently do this filtering in R
//		if(is_col_indexed())//filter out the PnX based on the col_idx
//		{
//
//			    pat <- paste(to.del, collapse = "|")
//			    pat <- paste0("^\\$P(", pat , ")[A-Z]$")
//			    sel <- grep(pat, names(kw))
//		}
		return res;
	}
	/**
	 * extract the value of the single keyword by keyword name
	 *
	 * @param key keyword name
	 * @return keyword value as a string
	 */
	string get_keyword(const string & key) const
	{
		return get_cytoframe_ptr()->get_keyword(key);

	}

	/**
	 * set the value of the single keyword
	 * @param key keyword name
	 * @param value keyword value
	 */
	void set_keyword(const string & key, const string & value)
	{
		get_cytoframe_ptr()->set_keyword(key, value);
	}
	void set_keywords(const KEY_WORDS & keys){
		get_cytoframe_ptr()->set_keywords(keys);
	}

	void set_range(const string & colname, ColType ctype, pair<EVENT_DATA_TYPE, EVENT_DATA_TYPE> new_range){
		get_cytoframe_ptr()->set_range(colname, ctype, new_range);
	}
	/**
	 * the range of a specific column
	 * @param colname
	 * @param ctype the type of column
	 * @param rtype either RangeType::data or RangeType::instrument
	 * @return
	 */
	pair<EVENT_DATA_TYPE, EVENT_DATA_TYPE> get_range(const string & colname, ColType ctype, RangeType rtype) const
	{

		return get_cytoframe_ptr()->get_range(colname, ctype	, rtype);
	}

	const PDATA & get_pheno_data() const {return get_cytoframe_ptr()->get_pheno_data();}
	void set_pheno_data(const string & name, const string & value) {get_cytoframe_ptr()->set_pheno_data(name, value);}
	void set_pheno_data(const PDATA & _pd) {get_cytoframe_ptr()->set_pheno_data(_pd);}
	const vector<cytoParam> & get_params() const
	{
		return get_cytoframe_ptr()->get_params();
	}

	void set_params(const vector<cytoParam> & _params)
	{
		get_cytoframe_ptr()->set_params(_params);
	}

	/*subsetting*/

	void cols_(vector<string> colnames, ColType col_type);

	/**
	 *
	 * @param col_idx column index relative to view
	 */
	void cols_(uvec col_idx);
	void cols_(vector<unsigned> col_idx);

	void rows_(vector<unsigned> row_idx);

	void rows_(uvec row_idx);
	/**
	 * Corresponding to the original $Pn FCS TEXT
	 * @return
	 */
	vector<unsigned> get_original_col_ids() const;
	unsigned n_cols() const;
	/**
	 * get the number of rows(or events)
	 * @return
	 */
	unsigned n_rows() const;
	/**
	 * clear the row and column index
	 */
	void reset_view(){
		row_idx_.clear();
		col_idx_.clear();
		is_row_indexed_ = is_col_indexed_ = false;
	}
	/**
	 * Realize the delayed subsetting (i.e. cols() and rows()) operations to the underlying data
	 * and clear the view
	 */
	CytoFrameView copy_realized(const string & h5_filename = "") const
	{
		if(is_row_indexed_ && is_col_indexed_){
			return get_cytoframe_ptr()->copy(row_idx_, col_idx_, h5_filename);
		}else if(is_row_indexed_){
			return get_cytoframe_ptr()->copy(row_idx_, true, h5_filename);
		}else if(is_col_indexed_){
			return get_cytoframe_ptr()->copy(col_idx_, false, h5_filename);
		}else{
			return get_cytoframe_ptr()->copy(h5_filename);
		}
	}
	void set_data(const EVENT_DATA_VEC & data_in);
	EVENT_DATA_VEC get_data() const;

	CytoFrameView copy(const string & h5_filename = "") const;
};
}



#endif /* INST_INCLUDE_CYTOLIB_CYTOFRAMEVIEW_HPP_ */

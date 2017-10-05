/*
 * CytoFrame.hpp
 *
 *  Created on: Sep 18, 2017
 *      Author: wjiang2
 */

#ifndef INST_INCLUDE_CYTOLIB_CYTOFRAME_HPP_
#define INST_INCLUDE_CYTOLIB_CYTOFRAME_HPP_


#include "readFCSHeader.hpp"
#include "compensation.hpp"
enum class ColType {channel, marker, unknown};
enum class RangeType {instrument, data};
enum class FrameType {FCS, H5};



/**
 * The class representing a single FCS file
 */
class CytoFrame{
protected:
	int nEvents;
	KEY_WORDS keys;
	vector<cytoParam> params;
	unordered_map<string, int> channel_vs_idx;
	unordered_map<string, int> marker_vs_idx;
public:
	virtual ~CytoFrame(){};
	virtual void compensate(const compensation &)=0;
//	virtual void transform(const transformation &)=0;
	virtual void save(const string & filename, FrameType type)=0;
	virtual EVENT_DATA_TYPE * getData()=0;
	virtual EVENT_DATA_TYPE * getData(const string &, ColType)=0;
	virtual KEY_WORDS getKeywords();
	virtual string getKeyword(const string &);
	virtual void setKeyword(const string &, const string &);
	virtual int nCol();
	virtual int nRow();
	virtual bool isHashed();
	virtual void buildHash();
	virtual vector<string> getChannels();
	virtual vector<string> getMarkers();
	virtual int getColId(const string &, ColType);
	virtual void setChannel(const string &, const string &);
	virtual void setMarker(const string &, const string &);
	virtual pair<EVENT_DATA_TYPE, EVENT_DATA_TYPE> getRange(const string &, ColType type, RangeType);
};



#endif /* INST_INCLUDE_CYTOLIB_CYTOFRAME_HPP_ */
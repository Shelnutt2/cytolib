// Copyright 2019 Fred Hutchinson Cancer Research Center
// See the included LICENSE file for details on the licence that is granted to the user of this software.
#include <cytolib/GatingSet.hpp>
#include <cytolib/H5CytoFrame.hpp>
#include <cytolib/MemCytoFrame.hpp>
#include <cytolib/cytolibConfig.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;


namespace cytolib
{
	GatingHierarchyPtr GatingSet::get_first_gh() const
	{
		if(size() == 0)
			throw(range_error("Empty GatingSet!"));
		return begin()->second;
	}

	/**
	 * separate filename from dir to avoid to deal with path parsing in c++
	 * @param path the dir of filename
	 * @param is_skip_data whether to skip writing cytoframe data to pb. It is typically remain as default unless for debug purpose (e.g. re-writing gs that is loaded from legacy pb archive without actual data associated)
	 */
	void GatingSet::serialize_pb(string path, H5Option h5_opt, bool is_skip_data)
	{
		/*
		 * validity check for path
		 */
		string errmsg = "Not a valid GatingSet archiving folder! " + path + "\n";
		if(fs::exists(path))
		{
			if(!fs::is_empty(fs::path(path)))
			{
//				if(is_overwrite)
//				{
//					fs::remove_all(path);
//					fs::create_directory(path);
//				}
//				else
//				{
					fs::path gs_pb_file;
					unordered_set<string> h5_samples;
					unordered_set<string> pb_samples;
					for(auto & e : fs::directory_iterator(path))
					{
						fs::path p = e;
						string ext = p.extension().string();
						if(ext == ".gs")
						{
							string fn = p.stem().string();
							if(fn == uid_)
							{
								if(gs_pb_file.empty())
									gs_pb_file = p;
								else
									throw(domain_error(errmsg + "Multiple .gs files found for the same gs object!"));
							}
							else
							{
								  throw(domain_error(errmsg + "gs file not matched to GatingSet uid: " + p.string()));

							}

						}
						else if(ext == ".pb"||ext == ".h5")
						{
							string sample_uid = p.stem().string();
							if(find(sample_uid) == end())
							  throw(domain_error(errmsg + "file not matched to any sample in GatingSet: " + p.string()));
							else
							{
								if(ext == ".pb")
									pb_samples.insert(p.stem().string());
								else
									h5_samples.insert(p.stem().string());
							}

						}
						else
						  throw(domain_error(errmsg + "File not recognized: " + p.string()));

					}

					if(gs_pb_file.empty())
					{
						if(pb_samples.size()>0)
						{
							throw(domain_error(errmsg + "gs file missing"));
						}
					}

					for(const auto & it : ghs_)
					{
						auto sn = it.first;
						if(h5_samples.find(sn) == h5_samples.end())
							throw(domain_error(errmsg + "h5 file missing for sample: " + sn));
						/*
						 * when no pb present, treat it as valid dest folder where h5 were pre-written
						 * e.g. when save_gs with cdf = skip
						 */
						if(pb_samples.size()>0)
						{
							if(pb_samples.find(sn) == pb_samples.end())
								throw(domain_error(errmsg + "pb file missing for sample: " + sn));
						}
					}
//				}
			}
		}
		else
			fs::create_directory(path);

		// Verify that the version of the library that we linked against is
		// compatible with the version of the headers we compiled against.
		GOOGLE_PROTOBUF_VERIFY_VERSION;
		//init the output stream for gs
		string filename = (fs::path(path) / uid_).string() + ".gs";
		ofstream output(filename.c_str(), ios::out | ios::trunc | ios::binary);
		string buf;
		google::protobuf::io::StringOutputStream raw_output(&buf);

		//empty message for gs
		pb::GatingSet gs_pb;

		//ver
		gs_pb.set_cytolib_verion(CYTOLIB_VERSION);
		gs_pb.set_pb_verion(std::to_string(GOOGLE_PROTOBUF_VERSION));
		unsigned h5major,h5minor,h5rel;
		H5::H5Library::getLibVersion(h5major,h5minor,h5rel);
		string h5ver = std::to_string(h5major) + "." + std::to_string(h5minor) + "." +std::to_string(h5rel);
		gs_pb.set_h5_verion(h5ver);
		//uid
		gs_pb.set_guid(uid_);

		const vector<string> sample_names = get_sample_uids();
		for(auto & sn : sample_names)
		{
			gs_pb.add_samplename(sn);
		}

		//write gs message to stream
		bool success = writeDelimitedTo(gs_pb, raw_output);

		if (!success){
			google::protobuf::ShutdownProtobufLibrary();
			throw(domain_error("Failed to write GatingSet."));
		}
		output.write(&buf[0], buf.size());

		//write each gh as a separate message to stream due to the pb message size limit
		//we now go one step further to save each message to individual file
		//due to the single string buffer used by lite-message won't be enough to hold the all samples for large dataset
		for(auto & sn : sample_names)
		{
			string h5_filename = (fs::path(path) / sn).string() + ".h5";
			//init the output stream for gs
			string filename = (fs::path(path) / sn).string() + ".pb";
			ofstream output(filename.c_str(), ios::out | ios::trunc | ios::binary);
			string buf;
			google::protobuf::io::StringOutputStream raw_output(&buf);


			pb::GatingHierarchy pb_gh;
			getGatingHierarchy(sn)->convertToPb(pb_gh, h5_filename, h5_opt, is_skip_data);


			bool success = writeDelimitedTo(pb_gh, raw_output);
			if (!success)
			{
				google::protobuf::ShutdownProtobufLibrary();
				throw(domain_error("Failed to write GatingHierarchy."));
			}
			output.write(&buf[0], buf.size());

		}

				// Optional:  Delete all global objects allocated by libprotobuf.
		google::protobuf::ShutdownProtobufLibrary();
	}

	 /* constructor from the legacy archives (de-serialization)
	 * @param filename
	 * @param format
	 * @param isPB
	 */
	GatingSet::GatingSet(string pb_file, const GatingSet & gs_data):GatingSet()
	{
		GOOGLE_PROTOBUF_VERIFY_VERSION;
		ifstream input(pb_file.c_str(), ios::in | ios::binary);
		if (!input) {
			throw(invalid_argument("File not found.." ));
		} else{
			 pb::GatingSet pbGS;

			 input.seekg (0, input.end);
			 int length = input.tellg();
			 input.seekg (0, input.beg);
			 vector<char> buf(length);
			 input.read(buf.data(), length);

			 google::protobuf::io::ArrayInputStream raw_input(buf.data(), length);
			 //read gs message
			 bool success = readDelimitedFrom(raw_input, pbGS);

			if (!success) {
				throw(domain_error("Failed to parse GatingSet."));
			}

			//parse global trans tbl from message
			map<intptr_t, TransPtr> trans_tbl;

			for(int i = 0; i < pbGS.trans_tbl_size(); i++){
				const pb::TRANS_TBL & trans_tbl_pb = pbGS.trans_tbl(i);
				const pb::transformation & trans_pb = trans_tbl_pb.trans();
				intptr_t old_address = (intptr_t)trans_tbl_pb.trans_address();

				/*
				 * first two global trans do not need to be restored from archive
				 * since they use the default parameters
				 * simply add the new address
				 */

				switch(i)
				{
				case 0:
					trans_tbl[old_address] = TransPtr(new biexpTrans());
					break;
				case 1:
					trans_tbl[old_address] = TransPtr(new linTrans());
					break;
				default:
					{
						switch(trans_pb.trans_type())
						{
						case pb::PB_CALTBL:
							trans_tbl[old_address] = TransPtr(new transformation(trans_pb));
							break;
						case pb::PB_BIEXP:
							trans_tbl[old_address] = TransPtr(new biexpTrans(trans_pb));
							break;
						case pb::PB_FASIGNH:
							trans_tbl[old_address] = TransPtr(new fasinhTrans(trans_pb));
							break;
						case pb::PB_FLIN:
							trans_tbl[old_address] = TransPtr(new flinTrans(trans_pb));
							break;
						case pb::PB_LIN:
							trans_tbl[old_address] = TransPtr(new linTrans(trans_pb));
							break;
						case pb::PB_LOG:
							trans_tbl[old_address] = TransPtr(new logTrans(trans_pb));
							break;
						case pb::PB_LOGICLE:
							trans_tbl[old_address] = TransPtr(new logicleTrans(trans_pb));
							break;
						case pb::PB_SCALE:
							trans_tbl[old_address] = TransPtr(new scaleTrans(trans_pb));
							break;
						default:
							throw(domain_error("unknown type of transformation archive!"));
						}
					}
				}

			}
			/*
			 * recover the trans_global
			 */

//			for(int i = 0; i < pbGS.gtrans_size(); i++){
//				const pb::trans_local & trans_local_pb = pbGS.gtrans(i);
//				gTrans.push_back(trans_global(trans_local_pb, trans_tbl));
//			}
			//read gating hierarchy messages
			for(int i = 0; i < pbGS.samplename_size(); i++){
				string sn = pbGS.samplename(i);

				//gh message is stored as the same order as sample name vector in gs
				pb::GatingHierarchy gh_pb;
				bool success = readDelimitedFrom(raw_input, gh_pb);

				if (!success) {
					throw(domain_error("Failed to parse GatingHierarchy."));
				}
				//only add the sample that is present in gs_data(in case fs was subsetted when gs was archived in legacy pb)
				if(gs_data.find(sn)!=gs_data.end())
					add_GatingHierarchy(GatingHierarchyPtr(new GatingHierarchy(gh_pb, trans_tbl)), sn, false);
			}

			if(gs_data.size()>0)
			{
				set_cytoset(gs_data);
				/*
				 * update sample view from the data, this direct operation on sample_names_is typically not recommended
				 * and herer only done for legacy gs where fs was separately stored and its sample order can be different from pb
				 */
				sample_names_ = gs_data.get_sample_uids();
			}

		}

	}


	/*
	 * up to caller to free the memory
	 */
	GatingSet GatingSet::copy(bool is_copy_data, bool is_realize_data, const string & new_h5_dir) const{
		GatingSet gs;
		fs::path h5_dir;
		if(is_copy_data)
			h5_dir = gs.generate_h5_folder(fs::path(new_h5_dir).string());
		for(const string & sn : get_sample_uids())
		{
			GatingHierarchyPtr gh = getGatingHierarchy(sn);

			if(g_loglevel>=GATING_HIERARCHY_LEVEL)
				PRINT("\n... copying GatingHierarchy: "+sn+"... \n");
			gs.add_GatingHierarchy(gh->copy(is_copy_data, is_realize_data, (h5_dir/sn).string()), sn, is_copy_data);

		}

		return gs;
	}

	/*Defunct
	 * TODO:current version of this contructor is based on gating template ,simply copying
	 * compensation and transformation,more options can be allowed in future like providing different
	 * comp and trans
	 */
	GatingSet::GatingSet(const GatingHierarchy & gh_template,const GatingSet & cs, bool execute):GatingSet(){
		auto samples = cs.get_sample_uids();
		for(const string & sn : samples)
		{

			if(g_loglevel>=GATING_HIERARCHY_LEVEL)
				PRINT("\n... start cloning GatingHierarchy for: "+sn+"... \n");
			auto gh = gh_template.copy(false, false, "");
			auto cfv = cs.get_cytoframe_view(sn);
			string h5_filename = cfv.get_h5_file_path();
			if(h5_filename=="")
				throw(logic_error("in-memory version of cs is not supported!"));
			if(execute)
			{
				if(g_loglevel>=GATING_HIERARCHY_LEVEL)
					PRINT("\n... load flow data: "+sn+"... \n");
				MemCytoFrame fr = MemCytoFrame(*(cfv.get_cytoframe_ptr()));
				if(g_loglevel>=GATING_HIERARCHY_LEVEL)
					PRINT("\n... compensate: "+sn+"... \n");
				gh->compensate(fr);
				if(g_loglevel>=GATING_HIERARCHY_LEVEL)
					PRINT("\n... transform_data: "+sn+"... \n");
				// fr.scale_time_channel();
				gh->transform_data(fr);
				if(g_loglevel>=GATING_HIERARCHY_LEVEL)
					PRINT("\n... gating: "+sn+"... \n");
				gh->gating(fr, 0, true, true);
				if(g_loglevel>=GATING_HIERARCHY_LEVEL)
					PRINT("\n... save flow data: "+sn+"... \n");
				cfv.set_params(fr.get_params());
				cfv.set_keywords(fr.get_keywords());
				cfv.set_data(fr.get_data());
			}
			//attach to gh
			gh->set_cytoframe_view(cfv);
			add_GatingHierarchy(gh, sn, false);

		}

	}

	/**
	 * assign the flow data from the source gs
	 * @param gs typically it is a root-only GatingSet that only carries cytoFrames
	 */
	void GatingSet::set_cytoset(const GatingSet & gs){
		if(!gs.is_cytoFrame_only())
			throw(domain_error("The input gs is not data-only object! "));

		for(const string & sn : get_sample_uids())
		{
			const auto & it = gs.find(sn);
			if(it==gs.end())
				throw(domain_error("Sample '"  + sn + "' is missing from the data to be assigned!"));
			GatingHierarchy & gh = *ghs_[sn];
			gh.set_cytoframe_view(it->second->get_cytoframe_view_ref());
		}
	};

	/**
	 * Extract the ungated data
	 * @param node_path
	 * @return
	 */
	GatingSet GatingSet::get_cytoset(){
		GatingSet gs;

		for(const string & sn : get_sample_uids())
		{
			GatingHierarchyPtr gh = getGatingHierarchy(sn);

			gs.add_cytoframe_view(sn, gh->get_cytoframe_view_ref());
		}
		return gs;
	}

	/**
	 * extract gated data
	 * @param node_path
	 * @return a root-only GatingSet that carries the subsetted cytoframes
	 */
	GatingSet GatingSet::get_cytoset(string node_path){
		GatingSet gs;
		for(const string & sn : get_sample_uids())
		{
			GatingHierarchyPtr gh = getGatingHierarchy(sn);

			CytoFrameView & fr = gs.add_cytoframe_view(sn, gh->get_cytoframe_view());
			//subset by node
			auto u = gh->getNodeID(node_path);
			gh->check_ungated_bool_node(u);
			nodeProperties & node = gh->getNodeProperty(u);
			fr.rows_(node.getIndices_u());
		}
		return gs;
	}
	string GatingSet::generate_h5_folder(string h5_dir) const
	{
		h5_dir = (fs::path(h5_dir) / uid_).string();
		if(fs::exists(h5_dir))
			throw(domain_error(h5_dir + " already exists!"));
		if(!fs::create_directories(h5_dir))
			throw(domain_error("Failed to create directory: " + h5_dir));
		return h5_dir;
	}
	/**
	 * Retrieve the GatingHierarchy object from GatingSet by sample name.
	 *
	 * @param sampleName a string providing the sample name as the key
	 * @return a pointer to the GatingHierarchy object
	 */
	GatingHierarchyPtr GatingSet::getGatingHierarchy(string sample_uid) const
	{

		const_iterator it=ghs_.find(sample_uid);
		if(it==ghs_.end())
			throw(domain_error(sample_uid + " not found!"));
		else
			return it->second;
	}

	/**
	 *
	 * update channel information stored in GatingSet
	 * @param chnl_map the mapping between the old and new channel names
	 */
	void GatingSet::set_channels(const CHANNEL_MAP & chnl_map)
	{

		//update gh
		for(auto & it : ghs_){

				if(g_loglevel>=GATING_HIERARCHY_LEVEL)
					PRINT("\nupdate channels for GatingHierarchy:"+it.first+"\n");
				it.second->set_channels(chnl_map);
				//comp
			}

	}

	/**
	 * Subset by samples
	 * @param sample_uids
	 * @return
	 */
	GatingSet GatingSet::sub_samples(const vector<string> & sample_uids) const
	{
		GatingSet res(*this);
		res.sub_samples_(sample_uids);
		res.uid_ = generate_uid();
		return res;
	}

	/**
	 * Subset by samples (in place)
	 * @param sample_uids
	 * @return
	 */
	void GatingSet::sub_samples_(const vector<string> & sample_uids)
	{
		ghMap ghs_new;
		//validity check
		for(const auto & uid : sample_uids)
		{
			const auto & it = find(uid);
			if(it==end())
				throw(domain_error("The data to be assigned is missing sample: " + uid));
			else
				ghs_new[uid] = it->second;
		}
		sample_names_ = sample_uids;
		ghs_ = ghs_new;
	}

	/**
	 * Subet set by columns (in place)
	 * @param colnames
	 * @param col_type
	 * @return
	 */
	void GatingSet::cols_(vector<string> colnames, ColType col_type)
	{

		for(auto & it : ghs_)
		{
			if(!it.second->is_cytoFrame_only())
				throw(domain_error("Can't subset by cols when gh is not data-only object! "));
			it.second->get_cytoframe_view_ref().cols_(colnames, col_type);

		}
	}


	/**
	 * Add sample
	 * @param sample_uid
	 * @param frame_ptr
	 */
	CytoFrameView & GatingSet::add_cytoframe_view(string sample_uid, const CytoFrameView & frame_view){
		if(!is_cytoFrame_only())
			throw(domain_error("Can't add cytoframes to gs when it is not data-only object! "));
		//validity check
		auto res = channel_consistency_check<GatingSet, CytoFrameView>(*this, frame_view, sample_uid);
		GatingHierarchyPtr gh = add_GatingHierarchy(GatingHierarchyPtr(new GatingHierarchy(res)), sample_uid);
		return gh->get_cytoframe_view_ref();

	}

	/**
	 * update sample (move)
	 * @param sample_uid
	 * @param frame_ptr
	 */
	void GatingSet::update_cytoframe_view(string sample_uid, const CytoFrameView & frame_view){
		if(find(sample_uid) == end())
			throw(domain_error("Can't update the cytoframe since it doesn't exists: " + sample_uid));
		auto res = channel_consistency_check<GatingSet, CytoFrameView>(*this, frame_view, sample_uid);
		ghs_[sample_uid]->set_cytoframe_view(res);
	}

	/**
	 * Constructor from FCS files
	 * @param file_paths
	 * @param config
	 * @param is_h5
	 * @param h5_dir
	 * @param is_add_root whether add root node. When false, gs is used as cytoset without gating tree
	 */
	GatingSet::GatingSet(const vector<string> & file_paths, const FCS_READ_PARAM & config, bool is_h5, string h5_dir):GatingSet()
	{
		vector<pair<string,string>> map(file_paths.size());
		transform(file_paths.begin(), file_paths.end(), map.begin(), [](string i){return make_pair(path_base_name(i), i);});
		add_fcs(map, config, is_h5, h5_dir);
	}

	void GatingSet::add_fcs(const vector<pair<string,string>> & sample_uid_vs_file_path
			, const FCS_READ_PARAM & config, bool is_h5, string h5_dir, bool readonly)
	{

		fs::path h5_path;
		if(is_h5)
			h5_path = generate_h5_folder(h5_dir);
		for(const auto & it : sample_uid_vs_file_path)
		{


			CytoFramePtr fr_ptr(new MemCytoFrame(it.second,config));
			//set pdata
			fr_ptr->set_pheno_data("name", path_base_name(it.second));

			dynamic_cast<MemCytoFrame&>(*fr_ptr).read_fcs();
			if(is_h5)
			{
				string h5_filename = (h5_path/it.first).string() + ".h5";
				fr_ptr->write_h5(h5_filename);
				fr_ptr.reset(new H5CytoFrame(h5_filename, readonly));
			}

			add_cytoframe_view(it.first, CytoFrameView(fr_ptr));

		}
	}

	/**
	 * Update sample id
	 * @param _old
	 * @param _new
	 */
	void GatingSet::set_sample_uid(const string & _old, const string & _new){
		if(_old.compare(_new) != 0)
		{
			auto it = find(_new);
			if(it!=end())
				throw(range_error(_new + " already exists!"));
			it = find(_old);
			if(it==end())
				throw(range_error(_old + " not found!"));

			ghs_[_new] = it->second;
			erase(_old);

			//update sample view
			auto it1 = std::find(sample_names_.begin(), sample_names_.end(),_new);
			if(it1 != sample_names_.end())
				throw(range_error(_new + " already exists!"));
			it1 = std::find(sample_names_.begin(), sample_names_.end(),_old);
			if(it1==sample_names_.end())
				throw(range_error(_old + " not found!"));
			*it1 = _new;

		}

	};


};



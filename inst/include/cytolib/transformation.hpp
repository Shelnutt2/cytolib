/*
 * transformation.hpp
 *
 *  Created on: Apr 24, 2012
 *      Author: wjiang2
 */

#ifndef TRANSFORMATION_HPP_
#define TRANSFORMATION_HPP_
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include "calibrationTable.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/compare.hpp>


#include "global.hpp"

using namespace std;

namespace cytolib
{
#define CALTBL 0
#define LOG 1
#define LIN 2
#define FLIN 3
#define FASINH 4
//#define LOGICLE 1
#define BIEXP 5

/* case insensitive compare predicate*/
struct ciLessBoost : std::binary_function<std::string, std::string, bool>
{
    bool operator() (const std::string & s1, const std::string & s2) const {
        return lexicographical_compare(s1, s2, boost::is_iless());
    }
};

typedef map<std::string, std::string, ciLessBoost> CHANNEL_MAP;

struct coordinate
{
//	friend class boost::serialization::access;

	EVENT_DATA_TYPE x,y;
	coordinate(EVENT_DATA_TYPE _x,EVENT_DATA_TYPE _y){x=_x;y=_y;};
	coordinate(){};
	void convertToPb(pb::coordinate & coor_pb){
		coor_pb.set_x(x);
		coor_pb.set_y(y);
	};
	coordinate(const pb::coordinate & coor_pb):x(coor_pb.x()),y(coor_pb.y()){};
};
inline bool compare_x(coordinate i, coordinate j) { return i.x<j.x; }
inline bool compare_y(coordinate i, coordinate j) { return i.y<j.y; }

class transformation;
typedef shared_ptr<transformation> TransPtr;
class transformation{
protected:
	calibrationTable calTbl; //no longer have to store calTbl since we now can compute it from biexp even for mac workspace
	bool isGateOnly;
	unsigned short type;//could have been avoided if it is not required by R API getTransformation that needs to extract concrete transformation
	string name;
	string channel;
	bool isComputed;//this flag allow lazy computCalTbl/interpolation
public:
	/*
		 * if it is pure transformation object,then assume calibration is directly read from ws
		 * so there is no need to compute calibration
		 */

	transformation():isGateOnly(false),type(CALTBL),isComputed(true){}
	transformation(bool _isGate, unsigned short _type):isGateOnly(_isGate),type(_type),isComputed(true){}
	virtual ~transformation(){};
	virtual void transforming(EVENT_DATA_TYPE * input, int nSize){
		if(!calTbl.isInterpolated()){
			 /* calculate calibration table from the function
			 */
			if(!computed())
			{
				if(g_loglevel>=POPULATION_LEVEL)
					PRINT("computing calibration table...\n");
				computCalTbl();
			}

			if(!isInterpolated())
			{
				if(g_loglevel>=POPULATION_LEVEL)
					PRINT("spline interpolating...\n");
				interpolate();
			}
		}

		calTbl.transforming(input, nSize);

}

	virtual void computCalTbl(){};//dummy routine that does nothing
	virtual Spline_Coefs getSplineCoefs(){return calTbl.getSplineCoefs();};
	virtual void setCalTbl(calibrationTable _tbl){
		calTbl=_tbl;
	}

	virtual calibrationTable getCalTbl(){return calTbl;};
	virtual void interpolate(){calTbl.interpolate();};
	virtual bool isInterpolated(){return calTbl.isInterpolated();}
	virtual bool gateOnly(){return isGateOnly;};
	virtual void setGateOnlyFlag(bool _flag){isGateOnly=_flag;};
	virtual bool computed(){return isComputed;};
	virtual void setComputeFlag(bool _flag){isComputed=_flag;};
	virtual string getName(){return name;};
	virtual void setName(string _name){name=_name;};
	virtual string getChannel(){return channel;};
	virtual void setChannel(string _channel){channel=_channel;};
	virtual unsigned short getType(){return type;};
	virtual void setType(unsigned short _type){type=_type;};
	virtual TransPtr clone() const{return TransPtr(new transformation(*this));};
	transformation(const pb::transformation & trans_pb){
		isComputed = trans_pb.iscomputed();
		isGateOnly = trans_pb.isgateonly();
		type = trans_pb.type();
		name = trans_pb.name();
		channel = trans_pb.channel();
		/* For PB_BIEXP, caltbl was not saved during archiving and thus could be 0x0
		  and thus trans_pb.caltbl() call is supposed to fall back to return the one from default_instance, which should not be null.
		  However strange enough, in some circumstances (e.g. after save_gs() call), this default_instance_->caltbl_ does become null
		  which leads this pb auto generated accessor function to be unsafe to be invoked.
		  So we have to skip it in case of biexp. (we don't need to do it anyway)
		*/
		if(trans_pb.trans_type() != pb::PB_BIEXP)
			calTbl = calibrationTable(trans_pb.caltbl());
	}
	virtual void convertToPb(pb::transformation & trans_pb){

		trans_pb.set_isgateonly(isGateOnly);
		trans_pb.set_type(type);
		trans_pb.set_name(name);
		trans_pb.set_channel(channel);
		/*skip saving calibration table to save disk space, which means it needs to be always recalculated when load it back
		 	 Setting the flag to FALSE can only make sure it is recomputed properly for the APIs where the flag is checked first
			but it is not sufficient to prevent the segfault because the pointer to the caltbl in pb object is unset and somehow the
			default_instance_->caltbl_ can also be null due to some previous operations (i.e.`save_gs`). So ::pb::calibrationTable& transformation::caltbl
			becomes unsafe to call.
		*/
		if(type == BIEXP)
			trans_pb.set_iscomputed(false);
		else
		{
			trans_pb.set_iscomputed(isComputed);
			pb::calibrationTable * cal_pb = trans_pb.mutable_caltbl();
			calTbl.convertToPb(*cal_pb);
		}



	}

	virtual TransPtr  getInverseTransformation(){
		if(!calTbl.isInterpolated()){
			 /* calculate calibration table from the function
			 */
			if(!computed())
			{
				if(g_loglevel>=POPULATION_LEVEL)
					PRINT("computing calibration table...\n");
				computCalTbl();
			}

			if(!isInterpolated())
			{
				if(g_loglevel>=POPULATION_LEVEL)
					PRINT("spline interpolating...\n");
				interpolate();
			}
		}

		//clone the existing trans
		TransPtr  inverse = TransPtr(new transformation(*this));
		//make sure to reset type to avoid type-discrepancy because
		//it returns the base transformation type instead of the original one (e.g. biexp)
		inverse->type = CALTBL;

		//swap the x, y vectors in calTbl

		inverse->calTbl.setX(this->calTbl.getY());
		inverse->calTbl.setY(this->calTbl.getX());

		//re-interpolate the inverse calibration tbl
		inverse->calTbl.setInterpolated(false);
		if(g_loglevel>=POPULATION_LEVEL)
				PRINT("spline interpolating...\n");
		inverse->interpolate();
		return inverse;
	}

	virtual void setTransformedScale(int scale){throw(domain_error("setTransformedScale function not defined!"));};
	virtual int getTransformedScale(){throw(domain_error("getTransformedScale function not defined!"));};
	virtual int getRawScale(){throw(domain_error("getRawScale function not defined!"));};
};
typedef shared_ptr<transformation> TransPtr;
/*
 * directly translated from java routine from tree star
 */
inline EVENT_DATA_TYPE logRoot(EVENT_DATA_TYPE b, EVENT_DATA_TYPE w)
{
	EVENT_DATA_TYPE xLo = 0;
	EVENT_DATA_TYPE xHi = b;
	EVENT_DATA_TYPE d = (xLo + xHi) / 2;
	EVENT_DATA_TYPE dX = abs((long) (xLo - xHi));
	EVENT_DATA_TYPE dXLast = dX;
	EVENT_DATA_TYPE fB = -2 * log(b) + w * b;
	EVENT_DATA_TYPE f = 2. * log(d) + w * b + fB;
	EVENT_DATA_TYPE dF = 2 / d + w;
	if (w == 0) return b;
	for (long i = 0; i < 100; i++)
	{
		if (((d - xHi) * dF - f) * ((d - xLo) * dF - f) >= 0 ||
				abs((long) (2 * f)) > abs((long) (dXLast * dF)))
		{
			dX = (xHi - xLo) / 2;
			d = xLo + dX;
			if (d == xLo)
				return d;
		}
		else
		{
			dX = f / dF;
			EVENT_DATA_TYPE t = d;
			d -= dX;
			if (d == t)
				return d;
		}
		if (abs((long) dX) < 1.0e-12)
			return d;
		dXLast = dX;
		f = 2 * log(d) + w * d + fB;
		dF = 2 / d + w;
		if (f < 0) xLo = d;
		else xHi = d;
	}
	return d;
}


class biexpTrans:public transformation{
public:
	int channelRange;
	EVENT_DATA_TYPE pos, neg, widthBasis, maxValue;
public:

	biexpTrans():transformation(false, BIEXP),channelRange(4096), pos(4.5), neg(0), widthBasis(-10),maxValue(262144){
		setComputeFlag(false);
		calTbl.setInterpolated(false);
	}

	 /*
	  * directly translated from java routine from tree star
	  * potential segfault risk: the inappropriate biexp parameters can cause
	  * the indexing of vector out of the boundary.
	  */

	void computCalTbl(){
		/*
		 * directly translated from java routine from tree star
		 */

		EVENT_DATA_TYPE ln10 = log(10.0);
		EVENT_DATA_TYPE decades = pos;
		EVENT_DATA_TYPE lowScale = widthBasis;
		EVENT_DATA_TYPE width = log10(-lowScale);

		if (width < 0.5 || width > 3) width = 0.5;
		decades -= width / 2;
		EVENT_DATA_TYPE extra = neg;
		if (extra < 0) extra = 0;
		extra += width / 2;

		int zeroChan = (int)(extra * channelRange / (extra + decades));
		zeroChan = min(zeroChan, channelRange / 2);

		if (zeroChan > 0) decades = extra * channelRange / zeroChan;
		width /= 2 * decades;        // 1.1

		EVENT_DATA_TYPE maximum = maxValue;
		EVENT_DATA_TYPE positiveRange = ln10 * decades;
		EVENT_DATA_TYPE minimum = maximum / exp(positiveRange);
		EVENT_DATA_TYPE negativeRange = logRoot(positiveRange, width);

		EVENT_DATA_TYPE maxChannlVal = channelRange + 1;
		int nPoints = maxChannlVal;//4097;//fix the number of points so that it won't lost the precision when scale is set to 256 (i.e. channelRange = 256)

		vector<EVENT_DATA_TYPE> positive(nPoints), negative(nPoints), vals(nPoints);
		EVENT_DATA_TYPE step = (maxChannlVal-1)/(EVENT_DATA_TYPE)(nPoints -1);
		for (int j = 0; j < nPoints; j++)
		{
			vals[j] = j * step;
			positive[j] = exp((float)(j) / (float)(nPoints) * positiveRange);
			negative[j] = exp((float)(j) / (float)(nPoints) * (-negativeRange));
		}



		EVENT_DATA_TYPE s = exp((positiveRange + negativeRange) * (width + extra / decades));
		for(int j = 0; j < nPoints; j++)
			negative[j] *= s;

		//ensure it is not out-of-bound
		if(zeroChan<0||zeroChan>=nPoints)
			throw(logic_error("invalid zeroChan: " + std::to_string(zeroChan)));

		s = positive[zeroChan] - negative[zeroChan];
		for (int j = zeroChan; j < nPoints; j++)
			positive[j] = minimum * (positive[j] - negative[j] - s);
		for (int j = 0; j < zeroChan; j++){
			int m = 2 * zeroChan - j;
			if(m<0||m>=nPoints)
					throw(logic_error("invalid value from '2 * zeroChan - j': " + std::to_string(m)));
			positive[j] = -positive[m];
		}


		/*
		 * save the calibration table
		 */
		calTbl.setCaltype("flowJo");
		calTbl.setMethod(2);
		calTbl.setX(positive);
		calTbl.setY(vals);

		isComputed=true;


	}

	TransPtr clone() const{return TransPtr(new biexpTrans(*this));};
	void convertToPb(pb::transformation & trans_pb){
		transformation::convertToPb(trans_pb);
		trans_pb.set_trans_type(pb::PB_BIEXP);
		pb::biexpTrans * bt_pb = trans_pb.mutable_bt();
		bt_pb->set_channelrange(channelRange);
		bt_pb->set_maxvalue(maxValue);
		bt_pb->set_neg(neg);
		bt_pb->set_pos(pos);
		bt_pb->set_widthbasis(widthBasis);
	}
	biexpTrans(const pb::transformation & trans_pb):transformation(trans_pb){
		if(!trans_pb.has_bt())
			throw(domain_error("biexpTrans field not found in pb::transformation!"));
		const pb::biexpTrans & bt_pb = trans_pb.bt();

		channelRange = bt_pb.channelrange();
		maxValue = bt_pb.maxvalue();
		neg = bt_pb.neg();
		pos = bt_pb.pos();
		widthBasis = bt_pb.widthbasis();

		//make sure to always recompute caltbl (regardless of compute flag) since it was not saved for the sake of space
	//	computCalTbl(); //now we do lazy-compute since some global trans (in the legacy ws and these trans are not actually used by any samples) may not have the valid parameters which will fail the pb unarchiving process
	}
	void setTransformedScale(int scale){
		channelRange = scale;
		//recompute cal table
		computCalTbl();
		interpolate();
	};
	int getTransformedScale(){return channelRange;};
	int getRawScale(){return maxValue;};

};

class fasinhTrans:public transformation{
public:
	EVENT_DATA_TYPE maxRange;
	EVENT_DATA_TYPE length;//unused at this moment
	EVENT_DATA_TYPE T, A, M;
public:
	fasinhTrans():transformation(false,FASINH),maxRange(262144),length(256), T(262144),A(0),M(4.5){
		calTbl.setInterpolated(true);
	}

	fasinhTrans(EVENT_DATA_TYPE _maxRange, EVENT_DATA_TYPE _length, EVENT_DATA_TYPE _T, EVENT_DATA_TYPE _A, EVENT_DATA_TYPE _M):transformation(false, FASINH),maxRange(_maxRange),length(_length), T(_T),A(_A),M(_M){
		calTbl.setInterpolated(true);
	}
	/*
	 * implementation copied from flowCore
	 */
	virtual void transforming(EVENT_DATA_TYPE * input, int nSize){


		for(int i=0;i<nSize;i++){
			input[i] = length * (asinh(input[i] * sinh(M * log(10)) / T) + A * log(10)) / ((M + A) * log(10));
		}
	//		EVENT_DATA_TYPE myB = (M + A) * log(10);
	//		EVENT_DATA_TYPE myC = A * log(10);
	//		EVENT_DATA_TYPE myA = T / sinh(myB - myC);
	//		input = input / myA;
	//
	//		// This formula for the arcsinh loses significance when x is negative
	//		//Therefore we take advantage of the fact that sinh is an odd function
	//		input = abs(input);
	//
	//		input = log(input + sqrt(input * input + 1));
	//		result = rep(NA, times=length(asinhx))
	//		result[negative] = (myC - asinhx[negative]) / myB
	//		result[!negative] = (asinhx[!negative] + myC) / myB
	//		result








	}


	TransPtr clone() const{return TransPtr(new fasinhTrans(*this));};
	void convertToPb(pb::transformation & trans_pb){
		transformation::convertToPb(trans_pb);
		trans_pb.set_trans_type(pb::PB_FASIGNH);
		pb::fasinhTrans * ft_pb = trans_pb.mutable_ft();
		ft_pb->set_a(A);
		ft_pb->set_length(length);
		ft_pb->set_m(M);
		ft_pb->set_maxrange(maxRange);
		ft_pb->set_t(T);
	}
	fasinhTrans(const pb::transformation & trans_pb):transformation(trans_pb){
		const pb::fasinhTrans & ft_pb = trans_pb.ft();
		length = ft_pb.length();
		maxRange = ft_pb.maxrange();
		T = ft_pb.t();
		A = ft_pb.a();
		M = ft_pb.m();
	}
	TransPtr  getInverseTransformation();

	void setTransformedScale(int scale){maxRange = scale;};
	int getTransformedScale(){return maxRange;};
	int getRawScale(){return T;};
};
/*
 * inverse transformation of fasinhTrans
 */
class fsinhTrans:public fasinhTrans{

public:

	fsinhTrans():fasinhTrans(){}

	fsinhTrans(EVENT_DATA_TYPE _length, EVENT_DATA_TYPE _maxRange, EVENT_DATA_TYPE _T, EVENT_DATA_TYPE _A, EVENT_DATA_TYPE _M):fasinhTrans(_length,_maxRange, _T, _A, _M){}

	void  transforming(EVENT_DATA_TYPE * input, int nSize){
		for(int i=0;i<nSize;i++)
			input[i] = sinh(((M + A) * log(10)) * input[i]/length - A * log(10)) * T / sinh(M * log(10));

	}
	TransPtr getInverseTransformation(){throw(domain_error("inverse function not defined!"));};
//	fsinhTrans * clone(){return new fasinhTrans(*this);};
//	void convertToPb(pb::transformation & trans_pb);
//	fasinhTrans(const pb::transformation & trans_pb);
//	TransPtr getInverseTransformation();
//	void setTransformedScale(int scale){length = scale;};
};

inline TransPtr  fasinhTrans::getInverseTransformation(){
		return TransPtr(new fsinhTrans(length, maxRange, T, A , M));
	}

/*
 * TODO:right now set two flags to TRUE in the contructor to avoid doing cal table stuff,
 * we should consider redesign the classes so that logTrans does not share this extra feature from parent class
 */
class logTrans:public transformation{
public:
		EVENT_DATA_TYPE offset;
		EVENT_DATA_TYPE decade;
		unsigned scale;
		unsigned T; //top value; derived from keyword $PnR for each channel
public:
	logTrans():transformation(false,LOG),offset(0),decade(1), scale(1),T(262144){
		calTbl.setInterpolated(true);
	}


	logTrans(EVENT_DATA_TYPE _offset,EVENT_DATA_TYPE _decade, unsigned _scale, unsigned _T):transformation(false,LOG),offset(_offset),decade(_decade), scale(_scale),T(_T){
		calTbl.setInterpolated(true);
	}


	/*
	 *
	 *now we switch back to zero imputation instead of min value since
	 *when convert to R version of transformation function, the data is
	 *no available anymore, thus no way to specify this minvalue
	 *
	 */
	EVENT_DATA_TYPE flog(EVENT_DATA_TYPE x,EVENT_DATA_TYPE T,EVENT_DATA_TYPE _min) {

		EVENT_DATA_TYPE M=decade;
		return x>0?(log10(x/T)/M+offset):_min;
	//	return x>0?(log10((x+offset)/T)/M):_min;

	}

	void transforming(EVENT_DATA_TYPE * input, int nSize){


	//		EVENT_DATA_TYPE thisMax=input.max();//max val must be globally determined during xml parsing
			EVENT_DATA_TYPE thisMin=0;//input.min();

			for(int i=0;i<nSize;i++){
				input[i]=flog(input[i],T,thisMin) * scale;
			}

	}
	TransPtr clone() const{return TransPtr(new logTrans(*this));};
	void convertToPb(pb::transformation & trans_pb){
		transformation::convertToPb(trans_pb);
		trans_pb.set_trans_type(pb::PB_LOG);
		pb::logTrans * lt_pb = trans_pb.mutable_lt();
		lt_pb->set_decade(decade);
		lt_pb->set_offset(offset);
		lt_pb->set_t(T);
	}
	logTrans(const pb::transformation & trans_pb):transformation(trans_pb){
		const pb::logTrans & lt_pb = trans_pb.lt();
		decade = lt_pb.decade();
		offset = lt_pb.offset();
		T = lt_pb.t();
	}

	TransPtr  getInverseTransformation();

	void setTransformedScale(int _scale){scale = _scale;};
	int getTransformedScale(){return scale;};
	int getRawScale(){return T;};
};

class logInverseTrans:public logTrans{
public:
	logInverseTrans(EVENT_DATA_TYPE _offset,EVENT_DATA_TYPE _decade, unsigned _scale, unsigned _T):logTrans(_offset, _decade, _scale, _T){};
	void transforming(EVENT_DATA_TYPE * input, int nSize){


	//		EVENT_DATA_TYPE thisMax=input.max();
	//		EVENT_DATA_TYPE thisMin=0;//input.min();

			for(int i=0;i<nSize;i++){
				input[i]= pow(10, (input[i]/scale - 1) * decade) * T;
			}


	//		input=log10(input);

	}

};

inline TransPtr  logTrans::getInverseTransformation(){
		return TransPtr(new logInverseTrans(offset, decade,scale, T));
	}

class linTrans:public transformation{
public:
	linTrans():transformation(true,LIN){
	        calTbl.setInterpolated(true);
	}
	void transforming(EVENT_DATA_TYPE * input, int nSize){
		for(int i=0;i<nSize;i++)
	                input[i]*=64;
	}

		TransPtr clone() const{return TransPtr(new linTrans(*this));};
        void convertToPb(pb::transformation & trans_pb){
        	transformation::convertToPb(trans_pb);
        	trans_pb.set_trans_type(pb::PB_LIN);

        }
        linTrans(const pb::transformation & trans_pb):transformation(trans_pb){}
        TransPtr getInverseTransformation(){throw(domain_error("inverse function not defined!"));};
        void setTransformedScale(int scale){throw(domain_error("setTransformedScale function not defined!"));};

};

/*
 * This class is dedicated to scale the EllipsoidGate
 */
class scaleTrans:public linTrans{
	int t_scale; //transformed scale
	int r_scale; // raw scale

public:

	scaleTrans():linTrans(),t_scale(256), r_scale(262144){isGateOnly = true;}
	scaleTrans(int _t_scale, int _r_scale):linTrans(),t_scale(_t_scale), r_scale(_r_scale){isGateOnly = true;}

	void transforming(EVENT_DATA_TYPE * input, int nSize){
		for(int i=0;i<nSize;i++)
			input[i]*=(t_scale/(EVENT_DATA_TYPE)r_scale);
	}

	TransPtr clone() const{return TransPtr(new scaleTrans(*this));};

	TransPtr getInverseTransformation(){
		return TransPtr(new scaleTrans(r_scale, t_scale));//swap the raw and trans scale
	}

	void setTransformedScale(int _scale){t_scale = _scale;};

};



class flinTrans:public transformation{
	EVENT_DATA_TYPE min;
	EVENT_DATA_TYPE max;
public:
	flinTrans():transformation(false,FLIN),min(0),max(0){
		calTbl.setInterpolated(true);
	}
	flinTrans(EVENT_DATA_TYPE _minRange, EVENT_DATA_TYPE _maxRange):transformation(false,FLIN),min(_minRange),max(_maxRange){
		calTbl.setInterpolated(true);
	}

	EVENT_DATA_TYPE flin(EVENT_DATA_TYPE x){
		EVENT_DATA_TYPE T=max;
		EVENT_DATA_TYPE A=min;
		return (x+A)/(T+A);
	}


	void transforming(EVENT_DATA_TYPE * input, int nSize){

		for(int i=0;i<nSize;i++){
			input[i]=flin(input[i]);
		}

	}

	TransPtr clone() const{return TransPtr(new flinTrans(*this));};

	void convertToPb(pb::transformation & trans_pb){
		transformation::convertToPb(trans_pb);
		trans_pb.set_trans_type(pb::PB_FLIN);
		pb::flinTrans * ft_pb = trans_pb.mutable_flt();
		ft_pb->set_max(max);
		ft_pb->set_min(min);
	}
	flinTrans(const pb::transformation & trans_pb):transformation(trans_pb){
		const pb::flinTrans & ft_pb = trans_pb.flt();
		max = ft_pb.max();
		min = ft_pb.min();
	}
	TransPtr getInverseTransformation(){throw(domain_error("inverse function not defined!"));};
	void setTransformedScale(int scale){throw(domain_error("setTransformedScale function not defined!"));};

};
};

#endif /* TRANSFORMATION_HPP_ */

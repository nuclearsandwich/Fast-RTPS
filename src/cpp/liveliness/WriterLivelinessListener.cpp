/*************************************************************************
 * Copyright (c) 2014 eProsima. All rights reserved.
 *
 * This copy of eProsima RTPS is licensed to you under the terms described in the
 * EPROSIMARTPS_LIBRARY_LICENSE file included in this distribution.
 *
 *************************************************************************/

/**
 * @file WriterLivelinessListener.cpp
 */

#include "eprosimartps/liveliness/WriterLivelinessListener.h"
#include "eprosimartps/liveliness/WriterLiveliness.h"
#include "eprosimartps/common/types/SerializedPayload.h"
#include "eprosimartps/Participant.h"
//#include "eprosimartps/qos/ParameterList.h"
#include "eprosimartps/utils/RTPSLog.h"

#include "eprosimartps/Endpoint.h"
#include "eprosimartps/reader/StatefulReader.h"
using namespace eprosima::dds;

namespace eprosima {
namespace rtps {

WriterLivelinessListener::WriterLivelinessListener(WriterLiveliness* wl):
		mp_WriterLiveliness(wl)
{


}

WriterLivelinessListener::~WriterLivelinessListener()
{

}

typedef std::vector<WriterProxy*>::iterator WPIT;

void WriterLivelinessListener::onNewDataMessage()
{
	boost::lock_guard<Endpoint> guard(*(Endpoint*)this->mp_WriterLiveliness->mp_builtinParticipantMessageReader);
	pInfo(MAGENTA<< "Liveliness Reader:  onNewDataMessage"<<endl);
	CacheChange_t* change;
	GuidPrefix_t guidP;
	LivelinessQosPolicyKind livelinessKind;
	if(this->mp_WriterLiveliness->mp_builtinParticipantMessageReader->get_last_added_cache(&change))
	{
		//Check the serializedPayload:
		if(change->serializedPayload.length>0)
		{
			ParameterList_t param;
			param.m_cdrmsg.msg_endian = change->serializedPayload.encapsulation == PL_CDR_BE ? BIGEND:LITTLEEND;
			param.m_cdrmsg.length = change->serializedPayload.length;
			memcpy(param.m_cdrmsg.buffer,change->serializedPayload.data,param.m_cdrmsg.length);
			if(ParameterList::readParameterListfromCDRMsg(&param.m_cdrmsg,&param,NULL,NULL)>0)
			{
				if(!processParameterList(&param,&guidP,&livelinessKind))
					return;
			}
			else
				return;
		}
		else
		{
			if(!separateKey(change->instanceHandle,&guidP,&livelinessKind))
				return;
		}
		if(guidP == this->mp_WriterLiveliness->mp_participant->getGuid().guidPrefix)
		{
			pDebugInfo(MAGENTA<<"Message from own participant, ignoring"<<DEF<<endl;);
			return;
		}
		// Update liveliness depending on Key
		if(livelinessKind == AUTOMATIC_LIVELINESS_QOS)
		{
			for(WPIT it = this->mp_WriterLiveliness->m_remoteAutomaticLivelinessWriters.begin();
					it!=this->mp_WriterLiveliness->m_remoteAutomaticLivelinessWriters.end();++it)
			{
				if((*it)->param.remoteWriterGuid.guidPrefix == guidP)
					(*it)->assertLiveliness();
			}
		}
		else if (livelinessKind == MANUAL_BY_PARTICIPANT_LIVELINESS_QOS)
		{
			for(WPIT it = this->mp_WriterLiveliness->m_remoteManualByParticipantLivelinessWriters.begin();
					it!=this->mp_WriterLiveliness->m_remoteManualByParticipantLivelinessWriters.end();++it)
			{
				if((*it)->param.remoteWriterGuid.guidPrefix == guidP)
					(*it)->assertLiveliness();
			}
		}
	}
	return;
}


bool WriterLivelinessListener::processParameterList(ParameterList_t* param,GuidPrefix_t* guidP,LivelinessQosPolicyKind* liveliness)
{
	for(std::vector<Parameter_t*>::iterator it=param->m_parameters.begin();
			it!=param->m_parameters.end();++it)
	{
		switch((*it)->Pid)
		{
		case(PID_KEY_HASH):
		{
			ParameterKey_t* p = (ParameterKey_t*)(*it);
			return separateKey(p->key,guidP,liveliness);
		}
		default:
		{
			pWarning("WriterLivelinessListener: in this ParameterList should not be anything but the Key"<<endl;);
			break;
		}
		}
	}
	return false;
}

bool WriterLivelinessListener::separateKey(InstanceHandle_t& key,GuidPrefix_t* guidP,LivelinessQosPolicyKind* liveliness)
{
	for(uint8_t i=0;i<12;++i)
	{
		guidP->value[i] = key.value[i];
	}
	*liveliness = (LivelinessQosPolicyKind)key.value[15];
	return true;
}


} /* namespace rtps */
} /* namespace eprosima */

/* 
 * Copyright (C) 2015 WYSIWYD Consortium, European Commission FP7 Project ICT-612139
 * Authors: Grégoire Pointeau, Tobias Fischer, Maxime Petit
 * email:   greg.pointeau@gmail.com, t.fischer@imperial.ac.uk, m.petit@imperial.ac.uk
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later versions published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * wysiwyd/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/

#include "proactiveTagging.h"

using namespace yarp::os;
using namespace yarp::sig;
using namespace wysiwyd::wrdac;
using namespace std;

/*
* Send a rpc command to ABM to obtain the kinematicStructure of part of the body (assuming it has already been named)
* Send a rpc command to kinematicStructure if no kinematicStructure were provided
* input: name of the bodypart to be moved (e.g. index, thumb, ...) + body part type (e.g. left_arm, right_arm, ...)
*/
Bottle proactiveTagging::assignKinematicStructureByName(std::string sName, std::string sBodyPartType, bool forcingKS) {
    Bottle bOutput;

    //1. search through opc for the m_joint_number corresponding to the bodypart name
    iCub->opc->checkout();
    Entity* e = iCub->opc->getEntity(sName, true);
    if(!e) {
        yError() << "Could not get bodypart" << sName;
        iCub->say("Could not get bodypart" + sName);
        bOutput.addString("nack");
        return bOutput;
    }

    //Error if the name does NOT correspond to a bodypart
    if(!e->isType("bodypart")) {
        yError() << " error in proactiveTagging::assignKinematicStructureByName | for " << sName << " | " << sName << " is NOT a bodypart : no kinematicStructure are allowed!" ;
        bOutput.addString("error");
        bOutput.addString("NOT a bodypart : no kinematicStructure are allowed!");
        return bOutput;
    }
    Bodypart* BPentity = dynamic_cast<Bodypart*>(e);
    int BPjoint = BPentity->m_joint_number;

    //2. go through ABM to find a singleJointBabbling with the corresponding joint, retrieve the instance number
    Bottle bResultByJoint = assignKinematicStructureByJoint(BPjoint, sBodyPartType, forcingKS);

    if(bResultByJoint.get(0).asString() == "error") {
        yError() << bResultByJoint.toString();
        return bResultByJoint;
    }

    yInfo() << " [assignKinematicStructureByName] | for name " << sName << " | instance found : " << bResultByJoint.get(1).asInt() ;
    bOutput.addString("ack");
    bOutput.addInt(bResultByJoint.get(1).asInt());

    return bOutput;
}


/*
* Send a rpc command to ABM to obtain the kinematicStructure of part of the body (using the joint info)
* Send a rpc command to kinematicStructure if no kinematicStructure were provided
* input: joint of the bodypart to be moved (e.g. m_joint_number) + body part type (e.g. left_arm, right_arm, ...)
*/
Bottle proactiveTagging::assignKinematicStructureByJoint(int BPjoint, std::string sBodyPartType, bool forcingKS) {
    //1. extract instance from singleJointAction for joint BPjoint, from ABM
    Bottle bResult, bOutput;
    ostringstream osRequest;
    osRequest << "SELECT max(main.instance) FROM main, contentarg WHERE main.instance = contentarg.instance AND activityname = 'singleJointBabbling' AND main.begin = TRUE AND contentarg.role = 'limb' AND contentarg.argument = '" << BPjoint << "' ;";
    yInfo() << "assignKinematicStructureByJoint request: " << osRequest.str();
    bResult = iCub->getABMClient()->requestFromString(osRequest.str().c_str());

    if (bResult.toString() == "NULL") {
        yError() << " error in proactiveTagging::assignKinematicStructureByJoint | for joint " << BPjoint << " | No instance corresponding to singleJointBabbling for this part" ;
        bOutput.addString("error");
        bOutput.addString("No instance corresponding to singleJointBabbling for this part");
        return bOutput;
    }
    int ksInstance = atoi(bResult.get(0).asList()->get(0).asString().c_str());

    Bottle bResultCheckKS = checkForKinematicStructure(ksInstance, forcingKS);
    if(bResultCheckKS.get(0).asString() == "error") {
        return bResultCheckKS;
    }
    yInfo() << " [assignKinematicStructureByJoint] | for joint " << BPjoint << " | instance found : " << ksInstance;

    //WRITE IN OPC
    iCub->opc->checkout();
    list<Entity*> lEntities = iCub->opc->EntitiesCache();
    Bottle bListEntChanged;
    for (auto& entity: lEntities) //go through all entity
    {
        yInfo() << "Checking if entity " << entity->name() << " has entitytype = bodypart : ----> " << entity->entity_type() ;
        if (entity->entity_type() == "bodypart")                                             //check bodypart entity
        {
            //pb with the casting: BPtemp is empty
            Bodypart* BPtemp = dynamic_cast<Bodypart*>(entity);
            if(!BPtemp) {
                yDebug() << "Could not cast " << entity->name() << " to Bodypart";
                continue;
            }
            if(BPtemp->m_joint_number == BPjoint) {                                             //if corresponding joint : change it
                //BPtemp->m_kinStruct_instance = ksInstance;
                bListEntChanged.addString(BPtemp->name());
                yInfo() << "Change" << BPtemp->name() << "to kinematic instance" << ksInstance;
                iCub->opc->commit(BPtemp);

                break;
            }
        }
    }

    yInfo() << "Out of the loop for checking entity in OPC, number of entityChanged : " << bListEntChanged.size() ;

    if(bListEntChanged.isNull()){
        yWarning() << "assignKinematicStructureByJoint | for joint " << BPjoint << " | no bodypart has been found with this joint!";
        bOutput.addString("warning");
        bOutput.addString("no bodypart has been found with this joint!");
        return bOutput;
    }

    bOutput.addString("ack");
    bOutput.addInt(ksInstance);
    bOutput.addList() = bListEntChanged;

    return bOutput;
}


/*
* Check if the instance has KS, other order it if forcing in ON
* input: instance to check, bool forcingKS to allow or not KS generation
*/
Bottle proactiveTagging::checkForKinematicStructure(int instance, bool forcingKS) {

    //1. Check that the instance number has some augmented kinematicStructure images
    Bottle bOutput, bResult;
    ostringstream osRequest;
    osRequest << "SELECT main.instance FROM main, visualdata WHERE main.instance = " << instance << " and main.instance = visualdata.instance AND augmented = 'kinematic_structure';";
    bResult = iCub->getABMClient()->requestFromString(osRequest.str().c_str());

    //2.a if yes, assign it to the bodypart in the opc
    //ELSE
    // 2.b i) launch kinematicStructure if forcingKS = true, ii) go out with error/warning otherwise
    if (bResult.toString() == "NULL") {
        if (!forcingKS) { //Instance with no KS, no forcingKS: send an error
            yError() << "checkForKinematicStructure | for instance " << instance << " | No instance corresponding to singleJointBabbling for this part (forcingKS = false, no attempt to launch it)" ;
            bOutput.addString("error");
            bOutput.addString("No instance corresponding to singleJointBabbling for this part (forcingKS = false, no attempt to launch it)");
            return bOutput;
        } else { //Instance with no KS, forcingKS, launch and try again
            yWarning() << "checkForKinematicStructure | for instance " << instance << " | No instance corresponding to singleJointBabbling for this part (forcingKS = true, attentoing to launch it, this may take some time)" ;
            Bottle bResultKS = orderKinematicStructure(instance);
            if (bResultKS.get(0).asString() == "error") {
                return bResultKS;               
            } else {
                return checkForKinematicStructure(instance, forcingKS); //recursive : try again if KS generation was fine
            }
        }
    }

    bOutput.addString("ack");
    return bOutput;
}

/*
* Launch a kinematicStructure generation for the instance, using ABM
* input: instance for the KS generation
*/
Bottle proactiveTagging::orderKinematicStructure(int instance) {
    Bottle bOutputError, bCommandKs, bInstance, bActivity, bQuantity, bReplyFromABM;

    bInstance.addString("instance");
    bInstance.addInt(instance);

    bQuantity.addString("quantity");
    bQuantity.addInt(2);

    bActivity.addString("activity");
    bActivity.addString("singleJointBabbling");

    bCommandKs.addString("requestAugmentedImages");
    bCommandKs.addList() = bInstance;
    bCommandKs.addList() = bActivity;
    bCommandKs.addList() = bQuantity;

    yDebug() << "Send reqeust for augmented images to ABM" << bCommandKs.toString();
    bReplyFromABM = iCub->getABMClient()->rpcCommand(bCommandKs);
    yDebug() << " [orderKinematicStructure] Reply from ABM:" << bReplyFromABM.toString();

    if (bReplyFromABM.isNull()){
        yError() << "orderKinematicStructure | for instance " << instance << " | no reply from ABM : is ABM or KinematicStructure are running?";
        bOutputError.addString("error");
        bOutputError.addString("no reply from ABM : is ABM or KinematicStructure are running?");
        return bOutputError;
    } else if (bReplyFromABM.get(0).asString() == "nack") {
        yError() << "orderKinematicStructure | for instance " << instance << " | KinematicStructure was NOT successful";
        bOutputError.addString("error");
        bOutputError.addString("KinematicStructure was NOT successful");
        return bOutputError;
    }

    return bReplyFromABM;
}

/*
* Explore an unknown tactile entity (e.g. fingertips), when knowing the name
* @param: Bottle with (exploreTactileUnknownEntity entityType entityName) (eg: exploreUnknownEntity agent unknown_25)
* @return Bottle with the result (error or ack?)
*/
yarp::os::Bottle proactiveTagging::exploreTactileEntityWithName(Bottle bInput) {  
    Bottle bOutput;

    if (bInput.size() != 3)
    {
        yInfo() << " proactiveTagging::exploreTactileEntityWithName | Problem in input size.";
        bOutput.addString("Problem in input size");
        iCub->say("Error in input size explore tactile with name");
        return bOutput;
    }

    string sBodyPart = bInput.get(1).toString();
    string sName = bInput.get(2).toString();

    yInfo() << " EntityType : " << sBodyPart;

    //1. search through opc for the bodypart entity
    iCub->opc->checkout();
    Bodypart* BPentity = dynamic_cast<Bodypart*>(iCub->opc->getEntity(sName, true));
    if(!BPentity) {
        iCub->say("Could not cast to bodypart in tactile");
        yError() << "Could not cast to bodypart in tactile";
        bOutput.addString("nack");
        return bOutput;
    }

    if (!Network::isConnected(touchDetectorRpc.c_str(), portFromTouchDetector.getName().c_str())) {
	    if (!Network::connect(touchDetectorRpc.c_str(), portFromTouchDetector.getName().c_str())) {
		yWarning() << "TOUCH DETECTOR NOT CONNECTED: selfTagging will not work";
	    }
    }

    //2.Ask human to touch
    string sAsking = "I know how to move my " + getBodyPartNameForSpeech(sName) + ", but how does it feel when I touch something? Can you touch my " + getBodyPartNameForSpeech(sName) + " when I move it, please?";
    yInfo() << " sAsking: " << sAsking;
    iCub->lookAtPartner();
    iCub->say(sAsking, false);
    portFromTouchDetector.read(false); // clear buffer from previous readings

    yarp::os::Time::delay(1.0);

    //2.b Move also the bodypart to show it has been learnt.
    yInfo() << "Cast okay : name BP = " << BPentity->name();
    int joint = BPentity->m_joint_number;
    //send rpc command to bodySchema to move the corresponding part
    yInfo() << "Start bodySchema";
    double babbling_duration = 3.0;
    iCub->babbling(joint, babblingArm, babbling_duration);

    //3. Read until some tactile value are detected
    //TODO: Here, instead we should check the saliency given by pasar!
    bool gotTouch = false;
    Bottle *bTactile;
    int timeout = 0;
    while(!gotTouch && timeout<10) {
        bTactile = portFromTouchDetector.read(false);
        if(bTactile != NULL) {
            gotTouch = true;
            break;
        } else {
            yarp::os::Time::delay(0.5);
            timeout++;
        }
    }

    if(!gotTouch) {
        yError() << " error in proactiveTagging::exploreTactileEntityWithName | for " << sName << " | Touch not detected!" ;
        bOutput.addString("error");
        bOutput.addString("I did not feel any touch.");
        iCub->say("I did not feel any touch.", false);
        iCub->home();

        return bOutput;
    }

    //4. Assign m_tactile_number
    BPentity->m_tactile_number = bTactile->get(0).asInt();
    bOutput.addString("ack");
    bOutput.addInt(bTactile->get(0).asInt());
    yDebug() << "Assigned" << BPentity->name() << "(id)" << BPentity->opc_id() << "tactile_number to" << bTactile->get(0).asInt();
    iCub->opc->commit(BPentity);

    //4.Ask human to touch
    string sThank = " Thank you, now I know when I am touching object with my " + getBodyPartNameForSpeech(sName);
    yInfo() << " sThank: " << sThank;
    iCub->lookAtPartner();
    iCub->say(sThank);
    iCub->home();

    return bOutput;
}

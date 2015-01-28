#include <yarp/sig/all.h>
#include "autobiographicalMemory.h"

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace cv;

Bottle autobiographicalMemory::addImgProvider(const string &labelImgProvider, const string &portImgProvider)
{
    //prepare the ResultSet of the query and the reply
    Bottle bReply;

    if (mapImgProvider.find(labelImgProvider) == mapImgProvider.end()) //key not found
    {
        //creating imgReceiverPort for the current provider (DEPRECATED)
        //string portImgReceiver = "/" + getName() + "/images/" + labelImgProvider + "/in";

        //add the imgProvider and imgReceiver to the map
        mapImgProvider[labelImgProvider] = portImgProvider;
        mapImgReceiver[labelImgProvider] = new yarp::os::BufferedPort < yarp::sig::ImageOf<yarp::sig::PixelRgb> > ;

        bReply.addString("[ack]");
    }
    else { //key found
        cout << "ERROR : addImgProvider : " << labelImgProvider << " is already present!" << endl;
        bReply.addString("ERROR : addImgProvider : " + labelImgProvider + " is already present!");
    }

    //send the reply
    return bReply;
}

Bottle autobiographicalMemory::removeImgProvider(const string &labelImgProvider)
{
    //prepare the ResultSet of the query and the reply
    Bottle bReply;

    if (mapImgProvider.find(labelImgProvider) == mapImgProvider.end()) { //key not found
        cout << "ERROR : removeImgProvider : " << labelImgProvider << " is NOT present! " << endl;
        bReply.addString("ERROR : removeImgProvider : " + labelImgProvider + " is NOT present! ");
    }
    else { //key found
        mapImgProvider.erase(labelImgProvider);
        mapImgReceiver[labelImgProvider]->interrupt();
        mapImgReceiver[labelImgProvider]->close();
        mapImgReceiver.erase(labelImgProvider);
        bReply.addString("[ack]");
    }

    return bReply;
}

Bottle autobiographicalMemory::getImagesInfo(int instance) {
    Bottle bRequest, bOutput;
    ostringstream osArg;

    bRequest.addString("request");
    osArg << "SELECT MAX(frame_number) FROM images WHERE instance = " << instance << endl;
    bRequest.addString(osArg.str());
    bRequest = request(bRequest);
    if(bRequest.get(0).toString() != "NULL") {
        bOutput.addInt(atoi(bRequest.get(0).asList()->get(0).toString().c_str()));

        bRequest.clear();
        osArg.str("");
        bRequest.addString("request");
        osArg << "SELECT DISTINCT img_provider_port FROM images WHERE instance = " << instance << endl;
        bRequest.addString(osArg.str());
        bRequest = request(bRequest);

        bOutput.addList() = bRequest;
    } else { // no images for given instance
        bOutput.addString("0");
    }

    return bOutput;
}

//Ask for a single image of a precise instance and frame_number
//If several provider were used, a single image from each image provider will be sent
// Return : ack ( ((imageMeta1.1) (image1.1)) ((imageMeta1.2) (image1.2)) ((imageMeta1.3) (image1.3)) )
Bottle autobiographicalMemory::askImage(int instance, int frame_number, string provider_port)
{
    Bottle bSubOutput, bOutput, bRequest;
    ostringstream osArg;

    //export all the images from the instance into the temp folder
    exportImages(instance, frame_number, frame_number, provider_port);

    bRequest.addString("request");
    osArg << "SELECT relative_path, img_provider_port, time FROM images";
    osArg << " WHERE instance = " << instance;
    osArg << " AND frame_number = " << frame_number;
    if(provider_port!="") {
        osArg << " AND img_provider_port = '" << provider_port << "'";
    }
    osArg << " ORDER BY time" << endl;

    bRequest.addString(osArg.str());
    bRequest = request(bRequest);

    for (int i = 0; i < bRequest.size(); i++) {
        string relative_path = bRequest.get(i).asList()->get(0).toString();
        string imgProviderPort = bRequest.get(i).asList()->get(1).asString();
        string imgTime = bRequest.get(i).asList()->get(2).asString();
        //cout << "Create image " << i << " " << relative_path << endl;

        // create image in the /storePath/tmp/relative_path
        IplImage* img = cvLoadImage((storingPath + "/" + storingTmpSuffix + "/" + relative_path).c_str(), CV_LOAD_IMAGE_UNCHANGED);
        if (img == 0) {
            cout << "Image " << storingPath << "/" << storingTmpSuffix << "/" + relative_path << " could not be loaded.";
            bOutput.addString((storingPath + "/" + storingTmpSuffix + "/" + relative_path).c_str());
            return bOutput;
        }

        //create a yarp image
        cvCvtColor(img, img, CV_BGR2RGB);
        ImageOf<PixelRgb> temp;
        temp.resize(img->width, img->height);
        cvCopyImage(img, (IplImage *)temp.getIplImage());

        //for the current image of the actual image provider, we add meta information to the image
        Bottle bCurrentImageMeta;
        bCurrentImageMeta.addInt(instance);
        bCurrentImageMeta.addInt(frame_number);
        bCurrentImageMeta.addString(imgProviderPort);
        bCurrentImageMeta.addString(imgTime);

        Bottle bCurrentImage;
        bCurrentImage.addList() = bCurrentImageMeta;
        //use this copyPortable in order to be able to send the image as a bottle in the rpc port of the respond method
        yarp::os::Portable::copyPortable(temp, bCurrentImage.addList());
        bSubOutput.addList() = bCurrentImage;

        cvReleaseImage(&img);
    }

    bOutput.addString("ack");
    bOutput.addList() = bSubOutput;

    return bOutput;
}

Bottle autobiographicalMemory::connectImgProviders()
{
    Bottle bOutput;

    if (mapImgProvider.size() == 0){
        bOutput.addString("ERROR [connectImgProviders] the map is NULL");
        return bOutput;
    }

    for (std::map<string, string>::const_iterator it = mapImgProvider.begin(); it != mapImgProvider.end(); ++it)
    {
        string portImgReceiver = "/" + getName() + "/images/" + it->first + "/in";
        mapImgReceiver.find(it->first)->second->open(portImgReceiver);
        //it->second: port name of img Provider
        //mapImgReceiver.find(it->first)->second: portname of imgReceiver which correspond to the label of imgProvider
        //cout << "  [connectImgProvider] : trying to connect " << it->second << " with " <<  mapImgReceiver.find(it->first)->second->getName().c_str() << endl ;
        if (!Network::isConnected(it->second, mapImgReceiver.find(it->first)->second->getName().c_str())) {
            //cout << "Port is NOT connected : we will connect" << endl ;
            if (!Network::connect(it->second, mapImgReceiver.find(it->first)->second->getName().c_str())) {
                cout << "Error: Connection could not be setup" << endl;
                bOutput.addString(it->second);
            }
            //cout << "Connection from : " << it->second << endl ;
            //cout << "Connection to   : " << mapImgReceiver.find(it->first)->second->getName().c_str() << endl;
        }
        else {
            //cout << "Error: Connection already present!" << endl ;
        }
    }

    if (bOutput.size() == 0){
        bOutput.addString("ack");
    }

    return bOutput;
}

Bottle autobiographicalMemory::disconnectImgProviders()
{
    Bottle bOutput;
    bool isAllDisconnected = true;

    if (mapImgProvider.size() == 0){
        bOutput.addString("ERROR [disconnectImgProviders] the map is NULL");
        return bOutput;
    }

    for (std::map<string, string>::const_iterator it = mapImgProvider.begin(); it != mapImgProvider.end(); ++it)
    {
        //it->second: port name of img Provider
        //mapImgReceiver.find(it->first)->second: port name of imgReceiver which correspond to the label of imgProvider
        Network::disconnect(it->second, mapImgReceiver.find(it->first)->second->getName().c_str());
        if (Network::isConnected(it->second, mapImgReceiver.find(it->first)->second->getName().c_str())) {
            cout << "ERROR [disconnectImgProvider] " << it->second << " is NOT disconnected!";
            bOutput.addString(it->second);
            isAllDisconnected = false;
        } else {
            //cout << "[disconnectImgProvider] " << it->second << " successfully disconnected!"  << endl ;

            //Have to close/interrupt each time otherwise the port is not responsive anymore
            mapImgReceiver.find(it->first)->second->interrupt();
            mapImgReceiver.find(it->first)->second->close();
        }
    }

    if (isAllDisconnected == true){
        bOutput.addString("ack");
    }

    //cout << "[disconnectImgProvider] bOutput = {" << bOutput.toString().c_str() << "}" << endl ;
    return bOutput;
}

bool autobiographicalMemory::createImage(const string &fullPath, BufferedPort<ImageOf<PixelRgb> >* imgPort)
{
    //Extract the incoming images from yarp
    ImageOf<PixelRgb> *yarpImage = imgPort->read();
    //cout << "imgPort name : " << imgPort->getName() << endl ;

    if (yarpImage != NULL) { // check we actually got something
        //use opencv to convert the image and save it
        IplImage *cvImage = cvCreateImage(cvSize(yarpImage->width(), yarpImage->height()), IPL_DEPTH_8U, 3);
        cvCvtColor((IplImage*)yarpImage->getIplImage(), cvImage, CV_RGB2BGR);
        cvSaveImage(fullPath.c_str(), cvImage);

        //cout << "img created : " << fullPath << endl ;
        cvReleaseImage(&cvImage);
    }
    else {
        cout << "ERROR CANNOT SAVE: no image received for: " << imgPort->getName() << endl;
        return false;
    }

    return true;
}

bool autobiographicalMemory::sendImage(const string &fullPath)
{
    return sendImage(fullPath, &imagePortOut);
}

bool autobiographicalMemory::sendImage(const string &fullPath, BufferedPort<ImageOf<PixelRgb> >* imgPort)
{
    //cout << "Going to send : " << fullPath << endl;
    IplImage* img = cvLoadImage(fullPath.c_str(), CV_LOAD_IMAGE_UNCHANGED);
    if (img == 0)
        return false;

    //create a yarp image
    cvCvtColor(img, img, CV_BGR2RGB);
    ImageOf<PixelRgb> &temp = imgPort->prepare();
    temp.resize(img->width, img->height);
    cvCopyImage(img, (IplImage *)temp.getIplImage());

    //send the image
    imgPort->write();

    cvReleaseImage(&img);

    return true;
}

Bottle autobiographicalMemory::sendStreamImage(int instance, bool timingE)
{
    Bottle bReply;
    timingEnabled = timingE;
    openStreamImgPorts(instance);
    openSendContDataPorts(instance);
    int imageCount = exportImages(instance);
    streamStatus = "send"; //streamStatus changed (triggered in update())

    bReply.addString(streamStatus);
    bReply.addInt(imageCount);

    Bottle bRequest;
    ostringstream osArg;

    bRequest.addString("request");
    osArg << "SELECT DISTINCT img_provider_port FROM images WHERE instance = " << instance << endl;
    bRequest.addString(osArg.str());
    bRequest = request(bRequest);
    Bottle bImgProviders;
    for(int i = 0; i < bRequest.size() && bRequest.toString()!="NULL"; i++) {
        bImgProviders.addString(portPrefix + bRequest.get(i).asList()->get(0).asString().c_str());
    }

    bReply.addList() = bImgProviders;

    bRequest.clear();
    osArg.str("");

    bRequest.addString("request");
    osArg << "SELECT DISTINCT label_port FROM continuousdata WHERE instance = " << instance << endl;
    bRequest.addString(osArg.str());
    bRequest = request(bRequest);
    Bottle bContDataProviders;
    for(int i = 0; i < bRequest.size() && bRequest.toString()!="NULL"; i++) {
        bContDataProviders.addString(portPrefix + bRequest.get(i).asList()->get(0).asString().c_str());
    }

    bReply.addList() = bContDataProviders;

    return bReply;
}

int autobiographicalMemory::openStreamImgPorts(int instance)
{
    Bottle bRequest;
    ostringstream osArg;

    bRequest.addString("request");
    osArg << "SELECT DISTINCT img_provider_port FROM images WHERE instance = " << instance << endl;
    bRequest.addString(osArg.str());
    bRequest = request(bRequest);

    for (int i = 0; i < bRequest.size() && bRequest.toString()!="NULL"; i++) {
        string imgProviderPort = bRequest.get(i).asList()->get(0).asString();
        mapStreamImgPortOut[imgProviderPort] = new yarp::os::BufferedPort < yarp::sig::ImageOf<yarp::sig::PixelRgb> >;
        mapStreamImgPortOut[imgProviderPort]->open((portPrefix+imgProviderPort).c_str());

        Network::connect(portPrefix+imgProviderPort, "/yarpview"+portPrefix+imgProviderPort);
    }

    cout << "openStreamImgPorts just created " << mapStreamImgPortOut.size() << " ports." << endl;

    return mapStreamImgPortOut.size();
}

//store an image into the SQL db /!\ no lo_import/oid!! (high frequency streaming needed)
bool autobiographicalMemory::storeImage(int instance, int frame_number, const string &relativePath, const string &imgTime, const string &currentImgProviderPort)
{
    Bottle bRequest;
    ostringstream osArg;

    //sql request with instance and label, images are stored from their location
    bRequest.addString("request");
    osArg << "INSERT INTO images(instance, frame_number, relative_path, time, img_provider_port) VALUES (" << instance << ", '" << frame_number << "', '" << relativePath << "', '" << imgTime << "', '" << currentImgProviderPort << "' );";
    bRequest.addString(osArg.str());
    bRequest = request(bRequest);

    return true;
}

//fullSentence is only used in case forSingleInstance=true!
bool autobiographicalMemory::storeImageAllProviders(const string &synchroTime, bool forSingleInstance, string fullSentence) {
    bool allGood = true;
    //go through the ImgReceiver ports

    for (std::map<string, BufferedPort<ImageOf<PixelRgb> >*>::const_iterator it = mapImgReceiver.begin(); it != mapImgReceiver.end(); ++it)
    {
        //concatenation of the path to store
        stringstream imgInstanceString; imgInstanceString << imgInstance;

        stringstream imgName;
        if(forSingleInstance) {
            if (fullSentence == ""){
                fullSentence = imgLabel;
            }
            //take the full sentence, replace space by _ to have the img name
            replace(fullSentence.begin(), fullSentence.end(), ' ', '_');
            imgName << fullSentence << "_" << it->first << "." << imgFormat;

            string currentPathFolder = storingPath + "/"; currentPathFolder+=imgInstanceString.str();
            yarp::os::mkdir(currentPathFolder.c_str());
#ifdef __linux__
            chmod(currentPathFolder.c_str(), 0777);
#endif
        } else {
            imgName << imgLabel << imgNb << "_" << it->first << "." << imgFormat;
        }

        string relativeImagePath = imgInstanceString.str() + "/" + imgName.str();

        string imagePath = storingPath + "/" + relativeImagePath;
        if (!createImage(imagePath, it->second)) {
            cout << "Error in Update : image not created from " << it->first << endl;
            allGood = false;
        }
        else {
            //cout << "Store image " << imagePath << " in database." << endl;
            //create SQL entry, register the cam image in specific folder
            if(!storeImage(imgInstance, imgNb, relativeImagePath, synchroTime, mapImgProvider[it->first])) {
                allGood = false;
                cout << "Something went wrong storing image " << relativeImagePath << endl;
            }
        }
    }

    // only save storeOID if its a single image instance
    // for streaming, we take care of this in updateModule at the stream "end"
    if(forSingleInstance) {
        storeOID();
    }

    return allGood;
}

bool autobiographicalMemory::storeOID() {
    Bottle bRequest;
    ostringstream osStoreOIDReq;

    osStoreOIDReq << "SELECT \"time\", img_provider_port, relative_path FROM images WHERE img_oid IS NULL";
    bRequest = requestFromString(osStoreOIDReq.str());

    if(bRequest.size()>0 && bRequest.get(0).toString() != "NULL") {
        cout << "Storing image OID, this may take a while!" << endl;
    } else {
        return true;
    }

    for(int i = 0; i<bRequest.size(); i++) {
        string imgTime = bRequest.get(i).asList()->get(0).toString().c_str();
        string imgProviderPort = bRequest.get(i).asList()->get(1).toString().c_str();
        string imgRelativePath = bRequest.get(i).asList()->get(2).toString().c_str();

        string fullPath = storingPath + "/" + imgRelativePath;
        unsigned int new_img_oid = ABMDataBase->lo_import(fullPath.c_str());

        ostringstream osStoreOID;
        osStoreOID << "UPDATE images SET img_oid=" << new_img_oid;
        osStoreOID << " WHERE time='" << imgTime << "' and img_provider_port = '" << imgProviderPort << "'";

        requestFromString(osStoreOID.str());

        if(i%100==0 || i==bRequest.size() - 1) {
            cout << "Saved " << i+1 << " images out of " << bRequest.size() << endl;
        }
    }

    return true;
}

// exports all images given an instance
int autobiographicalMemory::exportImages(int instance, int fromFrame, int toFrame, string provider_port)
{
    Bottle bRequest;
    ostringstream osArg;

    //extract oid of all the images
    bRequest.addString("request");
    osArg << "SELECT img_oid, relative_path FROM images WHERE";
    osArg << " instance = " << instance;
    if(fromFrame!=-1) {
        osArg << " AND frame_number >= " << fromFrame;
    }
    if(toFrame!=-1) {
        osArg << " AND frame_number <= " << toFrame;
    }
    if(provider_port!="") {
        osArg << " AND img_provider_port = '" << provider_port << "'";
    }
    osArg << endl;
    bRequest.addString(osArg.str());
    bRequest = request(bRequest);

    if(bRequest.toString()!="NULL") {
        //export all the images corresponding to the instance to a tmp folder in order to be sent after (update())
        for (int i = 0; i < bRequest.size(); i++) {
            int imageOID = atoi(bRequest.get(i).asList()->get(0).toString().c_str());
            string relative_path = bRequest.get(i).asList()->get(1).toString();
            if(i==0) { // only create folder to store images once
                string folderName = storingPath + "/" + storingTmpSuffix + "/" + relative_path.substr(0, relative_path.find_first_of("/"));
                yarp::os::mkdir(folderName.c_str());
        #ifdef __linux__
                chmod(folderName.c_str(), 0777);
        #endif
            }
            cout << "Call exportImage with OID " << imageOID << " : " << storingPath << "/" << storingTmpSuffix << "/" << relative_path << endl;
            exportImage(imageOID, storingPath + "/" + storingTmpSuffix + "/" + relative_path);
        }

        return bRequest.size(); // return how many images were saved
    } else {
        return 0;
    }
}

//export (i.e. save) a stored image to hardrive, using oid to identify and the path wanted
int autobiographicalMemory::exportImage(int img_oid, const string &imgPath) {
    return ABMDataBase->lo_export(img_oid, imgPath.c_str());
}

unsigned int autobiographicalMemory::getImagesProviderCount(int instance) {
    Bottle bRequest;
    ostringstream osArg;

    bRequest.addString("request");
    osArg << "SELECT DISTINCT img_provider_port FROM images WHERE instance = " << instance;
    bRequest.addString(osArg.str());
    bRequest = request(bRequest);
    if(bRequest.size()>0 && bRequest.toString()!="NULL") {
        return bRequest.size();
    } else {
        return 0;
    }
}

long autobiographicalMemory::getTimeLastImage(int instance) {
    Bottle bRequest;
    ostringstream osArg;

    osArg << "SELECT CAST(EXTRACT(EPOCH FROM time-(SELECT time FROM images WHERE instance = " << instance << " ORDER BY time LIMIT 1)) * 1000000 as INT) as time_difference FROM images WHERE instance = " << instance << " ORDER BY time DESC LIMIT 1;";
    bRequest.addString("request");
    bRequest.addString(osArg.str());
    bRequest = request(bRequest);
    if(bRequest.size()>0 && bRequest.toString()!="NULL") {
        return atol(bRequest.get(0).asList()->get(0).asString().c_str());
    } else {
        return 0;
    }
}

Bottle autobiographicalMemory::getListImages(long updateTimeDifference) {
    // Find which images to send
    Bottle bListImages;
    bListImages.addString("request");
    ostringstream osArgImages;

    osArgImages << "SELECT * FROM (";
    osArgImages << "SELECT relative_path, img_provider_port, time, ";
    osArgImages << "CAST(EXTRACT(EPOCH FROM time-(SELECT time FROM images WHERE instance = '" << imgInstance << "' ORDER BY time LIMIT 1)) * 1000000 as INT) as time_difference ";
    osArgImages << "FROM images WHERE instance = '" << imgInstance << "' ORDER BY time) s ";
    if(timingEnabled) {
        osArgImages << "WHERE time_difference <= " << updateTimeDifference << " and time_difference > " << timeLastImageSent << " ORDER BY time DESC LIMIT " << imgProviderCount << ";";
    } else {
        osArgImages << "WHERE time_difference > " << timeLastImageSent << " ORDER BY time ASC LIMIT " << imgProviderCount << ";";
    }

    bListImages.addString(osArgImages.str());
    return request(bListImages);
}

Bottle autobiographicalMemory::testAugmentedImage(Bottle bInput) {
    Bottle toSend;
    // Testing to put a circle on an image
    // This is just for testing purposes, and your own module should implement this!

    // Bottle: ack ( ((imageMeta1.1) (image1.1)) ((imageMeta1.2) (image1.2)) ((imageMeta1.3) (image1.3)) )
    if(bInput.get(0).asString()!="ack") {
        Bottle bError;
        bError.addString("no ack in input bottle");
        return bError;
    }

    Bottle* bImagesWithMeta = bInput.get(1).asList(); // ((imageMeta1.1) (image1.1)) ((imageMeta1.2) (image1.2)) ((imageMeta1.3) (image1.3))

    //go through each of the subottles, one for each imageProvider
    for(int i = 0; i < bImagesWithMeta->size(); i++) {
        Bottle* bSingleImageWithMeta = bImagesWithMeta->get(i).asList(); // ((imageMeta1.1) (image1.1))

        Bottle* bImageMeta = bSingleImageWithMeta->get(0).asList(); // (imageMeta1.1)
        Bottle* bRawImage = bSingleImageWithMeta->get(1).asList(); //image1.1

        ImageOf<PixelRgb> yarpImage;
        yarp::os::Portable::copyPortable(*bRawImage, yarpImage);

        yarp::sig::draw::addCircle(yarpImage,PixelRgb(255,0,0),
        yarpImage.width()/2,yarpImage.height()/2,
        yarpImage.height()/4);

        Bottle bAugmentedImage;
        yarp::os::Portable::copyPortable(yarpImage, bAugmentedImage);

        Bottle bAugmentedImageWithMeta;
        bAugmentedImageWithMeta.addList() = *bImageMeta;
        bAugmentedImageWithMeta.addList() = bAugmentedImage;
        bAugmentedImageWithMeta.addString("circle");

        toSend.addList() = bAugmentedImageWithMeta;
   }

    addAugmentedImages(toSend);

    return toSend;
}

Bottle autobiographicalMemory::addAugmentedImages(Bottle bInput) {
    Bottle bReply;

    for(int i=0; i<bInput.size(); i++) {
        Bottle* bImageWithMeta = bInput.get(i).asList();

        // extract variables from bottle
        Bottle bMetaInformation = *bImageWithMeta->get(0).asList();

        cout << bMetaInformation.toString() << endl;

        string instanceString = (bMetaInformation.get(0)).toString();
        int instance = atoi(instanceString.c_str());
        string frameNumberString = (bMetaInformation.get(1)).toString();
        int frame_number = atoi(frameNumberString.c_str());
        string providerPort = (bMetaInformation.get(2)).asString().c_str();
        string time = (bMetaInformation.get(3)).asString().c_str();
        string augmentedLabel = bImageWithMeta->get(2).asString();

        string providerPortSpecifier = providerPort.substr(providerPort.find_last_of("/")+1);

        // save image from bottle to file
        ImageOf<PixelRgb> yarpImage;
        Bottle* bImage = bImageWithMeta->get(1).asList();
        yarp::os::Portable::copyPortable(*bImage, yarpImage);
        IplImage *cvImage = cvCreateImage(cvSize(yarpImage.width(), yarpImage.height()), IPL_DEPTH_8U, 3);
        cvCvtColor((IplImage*)yarpImage.getIplImage(), cvImage, CV_RGB2BGR);

        string folderName = storingPath + "/" + storingTmpSuffix + "/" + augmentedLabel;
        yarp::os::mkdir(folderName.c_str());
    #ifdef __linux__
        chmod(folderName.c_str(), 0777);
    #endif
        string fullPath = folderName + "/" + frameNumberString + "_" + providerPortSpecifier + "." + imgFormat;
        cvSaveImage(fullPath.c_str(), cvImage);
        cvReleaseImage(&cvImage);

        // insert image to database
        unsigned int img_oid = ABMDataBase->lo_import(fullPath.c_str());

        // insert new row in database
        string relativePath = instanceString + "/" + augmentedLabel + "_" + frameNumberString + "_" + providerPortSpecifier + "." + imgFormat;
        string fullProviderPort = providerPort + "/" + augmentedLabel;

        Bottle bRequest;
        ostringstream osArg;

        bRequest.addString("request");
        osArg << "INSERT INTO images(instance, frame_number, relative_path, time, img_provider_port, img_oid, augmented) VALUES (" << instance << ", '" << frame_number << "', '" << relativePath << "', '" << time << "', '" << fullProviderPort << "', '" << img_oid << "', '" << augmentedLabel << "');";
        bRequest.addString(osArg.str());
        bRequest = request(bRequest);
    }

    return bReply;
}
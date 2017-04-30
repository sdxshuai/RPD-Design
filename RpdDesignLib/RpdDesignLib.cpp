#include <opencv2/highgui/highgui.hpp>

#include "com_shengjie_Main.h"
#include "../RpdDesign/Tooth.h"
#include "../RpdDesign/Utilities.h"

jobject matToJMat(JNIEnv* const& env, const Mat& mat) {
	auto clsStrMat = "org/opencv/core/Mat";
	auto clsMat = env->FindClass(clsStrMat);
	auto matConstructor = env->GetMethodID(clsMat, "<init>", "()V");
	auto jMat = env->NewObject(clsMat, matConstructor);
	auto midGetNativeObjAddr = env->GetMethodID(clsMat, "getNativeObjAddr", "()J");
	auto matAddr = env->CallLongMethod(jMat, midGetNativeObjAddr);
	*reinterpret_cast<Mat*>(matAddr) = mat;
	return jMat;
}

Mat& jMatToMat(JNIEnv* const& env, const jobject& jMat) {
	auto clsStrMat = "org/opencv/core/Mat";
	auto clsMat = env->FindClass(clsStrMat);
	auto midGetNativeObjAddr = env->GetMethodID(clsMat, "getNativeObjAddr", "()J");
	auto matAddr = env->CallLongMethod(jMat, midGetNativeObjAddr);
	return *reinterpret_cast<Mat*>(matAddr);
}

JNIEXPORT jobject JNICALL Java_com_shengjie_Main_getRpdDesign__Lorg_apache_jena_ontology_OntModel_2Lorg_opencv_core_Mat_2(JNIEnv* env, jclass, jobject ontModel, jobject base) {
	vector<Tooth> teeth[nZones];
	Mat designImages[2], designImage = imread("base.png");
	analyzeBaseImage(jMatToMat(env, base), teeth, designImages);
	vector<Rpd*> rpds;
	queryRpds(env, ontModel, rpds);
	updateDesign(teeth, rpds, designImages, true, true);
	for (auto rpd = rpds.begin(); rpd < rpds.end(); ++rpd)
		delete *rpd;
	bitwise_and(designImages[0], designImages[1], designImage);
	return matToJMat(env, designImage);
}

JNIEXPORT jobject JNICALL Java_com_shengjie_Main_getRpdDesign__Lorg_apache_jena_ontology_OntModel_2(JNIEnv* env, jclass cls, jobject ontModel) {
	auto base = imread("base.png");
	return Java_com_shengjie_Main_getRpdDesign__Lorg_apache_jena_ontology_OntModel_2Lorg_opencv_core_Mat_2(env, cls, ontModel, matToJMat(env, base));
}
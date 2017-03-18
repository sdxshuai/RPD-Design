#include <opencv2/imgproc.hpp>

#include "Rpd.h"
#include "Tooth.h"
#include "Utilities.h"

Rpd::Position::Position(const int& zone, const int& ordinal) : zone(zone), ordinal(ordinal) {}

bool Rpd::Position::operator==(const Position& rhs) const { return zone == rhs.zone && ordinal == rhs.ordinal; }

bool Rpd::Position::operator<(const Position& rhs) const {
	if (zone < rhs.zone)
		return true;
	if (zone == rhs.zone)
		return ordinal < rhs.ordinal;
	return false;
}

Rpd::Position& Rpd::Position::operator++() {
	++ordinal;
	return *this;
}

Rpd::Position& Rpd::Position::operator--() {
	--ordinal;
	return *this;
}

Rpd::Rpd(const vector<Position>& positions) : positions_(positions) {}

void Rpd::queryPositions(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midStatementGetProperty, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, vector<Position>& positions, bool isEighthToothUsed[nZones], const bool& autoComplete) {
	auto teeth = env->CallObjectMethod(individual, midListProperties, opComponentPosition);
	auto count = 0;
	while (env->CallBooleanMethod(teeth, midHasNext)) {
		auto tooth = env->CallObjectMethod(teeth, midNext);
		auto zone = env->CallIntMethod(env->CallObjectMethod(tooth, midStatementGetProperty, dpToothZone), midGetInt) - 1;
		auto ordinal = env->CallIntMethod(env->CallObjectMethod(tooth, midStatementGetProperty, dpToothOrdinal), midGetInt) - 1;
		positions.push_back(Position(zone, ordinal));
		if (ordinal == nTeethPerZone)
			isEighthToothUsed[zone] = true;
		++count;
	}
	for (auto i = 0; i < count - 1; ++i)
		for (auto j = i + 1; j < count; ++j)
			if (positions[i] > positions[j])
				swap(positions[i], positions[j]);
	if (autoComplete && count == 1)
		positions.push_back(positions[0]);
}

RpdWithMaterial::RpdWithMaterial(const Material& material) : material_(material) {}

void RpdWithMaterial::queryMaterial(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midResourceGetProperty, const jobject& dpClaspMaterial, const jobject& individual, Material& claspMaterial) { claspMaterial = static_cast<Material>(env->CallIntMethod(env->CallObjectMethod(individual, midResourceGetProperty, dpClaspMaterial), midGetInt)); }

RpdWithDirection::RpdWithDirection(const Rpd::Direction& direction) : direction_(direction) {}

void RpdWithDirection::queryDirection(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midResourceGetProperty, const jobject& dpClaspTipDirection, const jobject& individual, Rpd::Direction& claspTipDirection) { claspTipDirection = static_cast<Rpd::Direction>(env->CallIntMethod(env->CallObjectMethod(individual, midResourceGetProperty, dpClaspTipDirection), midGetInt)); }

void RpdAsLingualBlockage::registerLingualBlockages(vector<Tooth> teeth[nZones]) const {
	if (positions_.size() == 2)
		if (positions_[0].zone == positions_[1].zone)
			registerLingualBlockages(teeth, positions_);
		else {
			registerLingualBlockages(teeth, {Position(positions_[0].zone, 0), positions_[0]});
			registerLingualBlockages(teeth, {Position(positions_[1].zone, 0), positions_[1]});
		}
	else {
		registerLingualBlockages(teeth, {positions_[0], positions_[1]});
		registerLingualBlockages(teeth, {positions_[2], positions_[3]});
	}
}

RpdAsLingualBlockage::RpdAsLingualBlockage(const vector<Position>& positions, const vector<LingualBlockage>& lingualBlockages) : Rpd(positions), lingualBlockages_(lingualBlockages) {}

RpdAsLingualBlockage::RpdAsLingualBlockage(const vector<Position>& positions, const LingualBlockage& lingualBlockage) : RpdAsLingualBlockage(positions, vector<LingualBlockage>{lingualBlockage}) {}

void RpdAsLingualBlockage::registerLingualBlockages(vector<Tooth> teeth[nZones], const vector<Position>& positions) const {
	for (auto position = positions[0]; position <= positions[1]; ++position) {
		auto& tooth = getTooth(teeth, position);
		if (lingualBlockages_[0] != MAJOR_CONNECTOR || tooth.getLingualBlockage() == NONE)
			tooth.setLingualBlockage(lingualBlockages_[0]);
	}
}

void RpdAsLingualBlockage::registerLingualBlockages(vector<Tooth> teeth[nZones], const deque<bool>& flags) const {
	for (auto i = 0; i < flags.size(); ++i)
		if (flags[i])
			getTooth(teeth, positions_[i]).setLingualBlockage(lingualBlockages_[i]);
}

void RpdWithLingualClaspArms::setLingualArms(bool hasLingualConfrontations[nZones][nTeethPerZone]) {
	for (auto i = 0; i < positions_.size(); ++i) {
		auto& position = positions_[i];
		hasLingualClaspArms_[i] = !hasLingualConfrontations[position.zone][position.ordinal];
	}
}

RpdWithLingualClaspArms::RpdWithLingualClaspArms(const vector<Position>& positions, const Material& material, const vector<Direction>& tipDirections) : RpdWithMaterial(material), RpdAsLingualBlockage(positions, tipDirectionsToLingualBlockages(tipDirections)), tipDirections_(tipDirections), hasLingualClaspArms_(deque<bool>(positions.size())) {}

RpdWithLingualClaspArms::RpdWithLingualClaspArms(const vector<Position>& positions, const Material& material, const Direction& tipDirection) : RpdWithLingualClaspArms(positions, material, vector<Direction>{tipDirection}) {}

void RpdWithLingualClaspArms::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	for (auto i = 0; i < positions_.size(); ++i)
		if (hasLingualClaspArms_[i])
			HalfClasp(positions_[i], material_, tipDirections_[i], HalfClasp::LINGUAL).draw(designImage, teeth);
}

void RpdWithLingualClaspArms::registerLingualBlockages(vector<Tooth> teeth[nZones]) const { RpdAsLingualBlockage::registerLingualBlockages(teeth, hasLingualClaspArms_); }

RpdWithLingualConfrontations::RpdWithLingualConfrontations(const vector<Position>& positions, const vector<Position>& lingualConfrontations) : RpdAsLingualBlockage(positions, MAJOR_CONNECTOR), lingualConfrontations_(lingualConfrontations) {}

void RpdWithLingualConfrontations::registerLingualConfrontations(bool hasLingualConfrontations[nZones][nTeethPerZone]) const {
	for (auto position = lingualConfrontations_.begin(); position < lingualConfrontations_.end(); ++position)
		hasLingualConfrontations[position->zone][position->ordinal] = true;
}

void RpdWithLingualConfrontations::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	for (auto position = lingualConfrontations_.begin(); position < lingualConfrontations_.end(); ++position)
		Plating({*position}).draw(designImage, teeth);
}

void RpdWithLingualConfrontations::queryLingualConfrontations(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midStatementGetProperty, const jobject& dpLingualConfrontation, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& individual, vector<Position>& lingualConfrontations) {
	auto lcTeeth = env->CallObjectMethod(individual, midListProperties, dpLingualConfrontation);
	while (env->CallBooleanMethod(lcTeeth, midHasNext)) {
		auto tooth = env->CallObjectMethod(lcTeeth, midNext);
		lingualConfrontations.push_back(Position(env->CallIntMethod(env->CallObjectMethod(tooth, midStatementGetProperty, dpToothZone), midGetInt) - 1, env->CallIntMethod(env->CallObjectMethod(tooth, midStatementGetProperty, dpToothOrdinal), midGetInt) - 1));
	}
}

AkerClasp::AkerClasp(const vector<Position>& positions, const Material& material, const Direction& direction) : RpdWithDirection(direction), RpdWithLingualClaspArms(positions, material, direction) {}

AkerClasp* AkerClasp::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midResourceGetProperty, const jmethodID& midStatementGetProperty, const jobject& dpClaspTipDirection, const jobject& dpClaspMaterial, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions;
	Direction claspTipDirection;
	Material claspMaterial;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed);
	queryDirection(env, midGetInt, midResourceGetProperty, dpClaspTipDirection, individual, claspTipDirection);
	queryMaterial(env, midGetInt, midResourceGetProperty, dpClaspMaterial, individual, claspMaterial);
	return new AkerClasp(positions, claspMaterial, claspTipDirection);
}

void AkerClasp::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	RpdWithLingualClaspArms::draw(designImage, teeth);
	OcclusalRest(positions_, direction_ == MESIAL ? DISTAL : MESIAL).draw(designImage, teeth);
	HalfClasp(positions_, material_, direction_, HalfClasp::BUCCAL).draw(designImage, teeth);
}

CombinationClasp::CombinationClasp(const vector<Position>& positions, const Direction& direction) : RpdWithDirection(direction), RpdWithLingualClaspArms(positions, CAST, direction) {}

CombinationClasp* CombinationClasp::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midResourceGetProperty, const jmethodID& midStatementGetProperty, const jobject& dpClaspTipDirection, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions;
	Direction claspTipDirection;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed);
	queryDirection(env, midGetInt, midResourceGetProperty, dpClaspTipDirection, individual, claspTipDirection);
	return new CombinationClasp(positions, claspTipDirection);
}

void CombinationClasp::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	RpdWithLingualClaspArms::draw(designImage, teeth);
	OcclusalRest(positions_, direction_ == MESIAL ? DISTAL : MESIAL).draw(designImage, teeth);
	HalfClasp(positions_, WROUGHT_WIRE, direction_, HalfClasp::BUCCAL).draw(designImage, teeth);
}

CombinedClasp::CombinedClasp(const vector<Position>& positions, const Material& material) : RpdWithLingualClaspArms(positions, material, {positions[0].zone == positions[1].zone ? MESIAL : DISTAL, DISTAL}) {}

CombinedClasp* CombinedClasp::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midResourceGetProperty, const jmethodID& midStatementGetProperty, const jobject& dpClaspMaterial, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions;
	Material claspMaterial;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed);
	queryMaterial(env, midGetInt, midResourceGetProperty, dpClaspMaterial, individual, claspMaterial);
	return new CombinedClasp(positions, claspMaterial);
}

void CombinedClasp::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	RpdWithLingualClaspArms::draw(designImage, teeth);
	auto isInSameZone = positions_[0].zone == positions_[1].zone;
	OcclusalRest(positions_[0], isInSameZone ? DISTAL : MESIAL).draw(designImage, teeth);
	OcclusalRest(positions_[1], MESIAL).draw(designImage, teeth);
	HalfClasp(positions_[0], material_, isInSameZone ? MESIAL : DISTAL, HalfClasp::BUCCAL).draw(designImage, teeth);
	HalfClasp(positions_[1], material_, DISTAL, HalfClasp::BUCCAL).draw(designImage, teeth);
}

DentureBase::DentureBase(const vector<Position>& positions) : RpdAsLingualBlockage(positions, DENTURE_BASE) {}

DentureBase* DentureBase::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midStatementGetProperty, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed, true);
	return new DentureBase(positions);
}

void DentureBase::determineTailsCoverage(const bool isEighthToothUsed[nZones]) {
	for (auto i = 0; i < 2; ++i)
		if (positions_[i].ordinal == nTeethPerZone - 1 + isEighthToothUsed[positions_[i].zone])
			coversTails_[i] = true;
}

void DentureBase::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	vector<Point> curve;
	float avgRadius;
	bool isBlockedByMajorConnector;
	computeStringCurve(teeth, positions_, curve, avgRadius, &isBlockedByMajorConnector);
	if (coversTails_[0]) {
		Point2f distalPoint = getTooth(teeth, positions_[0]).getAnglePoint(180);
		curve[0] = roundToInt(distalPoint + rotate(computeNormalDirection(distalPoint), -CV_PI / 2) * avgRadius);
	}
	if (coversTails_[1]) {
		Point2f distalPoint = getTooth(teeth, positions_[1]).getAnglePoint(180);
		curve.back() = roundToInt(distalPoint + rotate(computeNormalDirection(distalPoint), CV_PI * (positions_[1].zone % 2 - 0.5)) * avgRadius);
	}
	if (coversTails_[0] || coversTails_[1] || !isBlockedByMajorConnector) {
		vector<Point> tmpCurve(curve.size());
		for (auto i = 0; i < curve.size(); ++i) {
			auto delta = roundToInt(computeNormalDirection(curve[i]) * avgRadius * 1.75);
			tmpCurve[i] = curve[i] + delta;
			curve[i] -= delta;
		}
		curve.insert(curve.end(), tmpCurve.rbegin(), tmpCurve.rend());
		computeSmoothCurve(curve, curve, true);
		polylines(designImage, curve, true, 0, lineThicknessOfLevel[2], LINE_AA);
	}
	else {
		curve.insert(curve.begin(), curve[0]);
		curve.push_back(curve.back());
		for (auto i = 1; i < curve.size() - 1; ++i)
			curve[i] += roundToInt(computeNormalDirection(curve[i]) * avgRadius * 1.75);
		computeSmoothCurve(curve, curve);
		polylines(designImage, curve, false, 0, lineThicknessOfLevel[2], LINE_AA);
	}
}

void DentureBase::registerLingualBlockages(vector<Tooth> teeth[nZones]) const {
	if (coversTails_[0] || coversTails_[1])
		RpdAsLingualBlockage::registerLingualBlockages(teeth);
}

EdentulousSpace::EdentulousSpace(const vector<Position>& positions) : Rpd(positions) {}

EdentulousSpace* EdentulousSpace::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midStatementGetProperty, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed, true);
	return new EdentulousSpace(positions);
}

void EdentulousSpace::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	vector<Point> curve;
	float avgRadius;
	computeStringCurve(teeth, positions_, curve, avgRadius);
	vector<Point> curve1(curve.size()), curve2(curve.size());
	for (auto i = 0; i < curve.size(); ++i) {
		auto delta = roundToInt(computeNormalDirection(curve[i]) * avgRadius / 4);
		curve1[i] = curve[i] + delta;
		curve2[i] = curve[i] - delta;
	}
	computeSmoothCurve(curve1, curve1);
	computeSmoothCurve(curve2, curve2);
	polylines(designImage, curve1, false, 0, lineThicknessOfLevel[2], LINE_AA);
	polylines(designImage, curve2, false, 0, lineThicknessOfLevel[2], LINE_AA);
}

LingualRest::LingualRest(const vector<Position>& positions, const Direction& direction) : Rpd(positions), RpdWithDirection(direction) {}

LingualRest* LingualRest::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midResourceGetProperty, const jmethodID& midStatementGetProperty, const jobject& dpRestMesialOrDistal, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions;
	Direction restMesialOrDistal;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed);
	queryDirection(env, midGetInt, midResourceGetProperty, dpRestMesialOrDistal, individual, restMesialOrDistal);
	return new LingualRest(positions, restMesialOrDistal);
}

void LingualRest::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	auto curve = getTooth(teeth, positions_[0]).getCurve(240, 300);
	polylines(designImage, curve, true, 0, lineThicknessOfLevel[1], LINE_AA);
	fillPoly(designImage, vector<vector<Point>>{curve}, 0, LINE_AA);
}

OcclusalRest::OcclusalRest(const vector<Position>& positions, const Direction& direction) : Rpd(positions), RpdWithDirection(direction) {}

OcclusalRest::OcclusalRest(const Position& position, const Direction& direction) : OcclusalRest(vector<Position>{position}, direction) {}

OcclusalRest* OcclusalRest::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midResourceGetProperty, const jmethodID& midStatementGetProperty, const jobject& dpRestMesialOrDistal, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions;
	Direction restMesialOrDistal;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed);
	queryDirection(env, midGetInt, midResourceGetProperty, dpRestMesialOrDistal, individual, restMesialOrDistal);
	return new OcclusalRest(positions, restMesialOrDistal);
}

void OcclusalRest::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	auto& tooth = getTooth(teeth, positions_[0]);
	Point2f point;
	vector<Point> curve;
	if (direction_ == MESIAL) {
		point = tooth.getAnglePoint(0);
		curve = tooth.getCurve(340, 20);
	}
	else {
		point = tooth.getAnglePoint(180);
		curve = tooth.getCurve(160, 200);
	}
	auto& centroid = tooth.getCentroid();
	curve.push_back(centroid + (point - centroid) / 2);
	polylines(designImage, curve, true, 0, lineThicknessOfLevel[1], LINE_AA);
	fillPoly(designImage, vector<vector<Point>>{curve}, 0, LINE_AA);
}

PalatalPlate* PalatalPlate::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midStatementGetProperty, const jobject& dpAnchorMesialOrDistal, const jobject& dpLingualConfrontation, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& opMajorConnectorAnchor, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions, lingualConfrontations;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed);
	queryLingualConfrontations(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpLingualConfrontation, dpToothZone, dpToothOrdinal, individual, lingualConfrontations);
	return new PalatalPlate(positions, lingualConfrontations);
}

PalatalPlate::PalatalPlate(const vector<Position>& positions, const vector<Position>& lingualConfrontations) : RpdWithLingualConfrontations(positions, lingualConfrontations) {}

void PalatalPlate::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	/*TODO :  consider denture base*/
	RpdWithLingualConfrontations::draw(designImage, teeth);
	auto ordinal = max(positions_[2].ordinal, positions_[0].ordinal);
	auto position1 = Position(positions_[2].zone, ordinal), position2 = Position(positions_[0].zone, ordinal);
	vector<Point> mesialCurve, distalCurve, curve1, curve2;
	float sumOfRadii = 0;
	auto nTeeth = 0;
	computeStringCurve(teeth, {positions_[2], position1}, curve1, sumOfRadii, nTeeth);
	curve1.insert(curve1.begin(), curve1[0]);
	computeStringCurve(teeth, {positions_[0], position2}, curve2, sumOfRadii, nTeeth);
	curve2.insert(curve2.begin(), curve2[0]);
	auto avgRadius = sumOfRadii / nTeeth;
	for (auto point = curve1.begin() + 1; point < curve1.end(); ++point)
		*point -= roundToInt(computeNormalDirection(*point) * avgRadius * 1.75);
	for (auto point = curve2.begin() + 1; point < curve2.end(); ++point)
		*point -= roundToInt(computeNormalDirection(*point) * avgRadius * 1.75);
	mesialCurve.insert(mesialCurve.end(), curve1.begin(), curve1.end() - 1);
	mesialCurve.push_back((curve1.back() + curve2.back()) / 2);
	mesialCurve.insert(mesialCurve.end(), curve2.rbegin() + 1, curve2.rend());
	computeSmoothCurve(mesialCurve, mesialCurve);
	ordinal = min(positions_[1].ordinal, positions_[3].ordinal);
	position1 = Position(positions_[1].zone, ordinal) , position2 = Position(positions_[3].zone, ordinal);
	sumOfRadii = nTeeth = 0;
	computeStringCurve(teeth, {position1, positions_[1]}, curve1, sumOfRadii, nTeeth);
	curve1.push_back(curve1.back());
	computeStringCurve(teeth, {position2, positions_[3]}, curve2, sumOfRadii, nTeeth);
	curve2.push_back(curve2.back());
	avgRadius = sumOfRadii / nTeeth;
	for (auto point = curve1.begin(); point < curve1.end() - 1; ++point)
		*point -= roundToInt(computeNormalDirection(*point) * avgRadius * 1.75);
	for (auto point = curve2.begin(); point < curve2.end() - 1; ++point)
		*point -= roundToInt(computeNormalDirection(*point) * avgRadius * 1.75);
	distalCurve.insert(distalCurve.end(), curve1.rbegin(), curve1.rend() - 1);
	distalCurve.push_back((curve1[0] + curve2[0]) / 2);
	distalCurve.insert(distalCurve.end(), curve2.begin() + 1, curve2.end());
	computeSmoothCurve(distalCurve, distalCurve);
	vector<Point> curve, tmpCurve;
	vector<vector<Point>> curves;
	computeLingualCurve(teeth, {positions_[0] , positions_[1]}, tmpCurve, curves);
	curve.insert(curve.end(), tmpCurve.begin(), tmpCurve.end());
	curve.insert(curve.end(), distalCurve.begin(), distalCurve.end());
	computeLingualCurve(teeth, {positions_[2] , positions_[3]}, tmpCurve, curves);
	curve.insert(curve.end(), tmpCurve.rbegin(), tmpCurve.rend());
	curve.insert(curve.end(), mesialCurve.begin(), mesialCurve.end());
	auto thisDesign = Mat(designImage.size(), CV_8U, 255);
	fillPoly(thisDesign, vector<vector<Point>>{curve}, 128, LINE_AA);
	bitwise_and(thisDesign, designImage, designImage);
	for (auto i = 0; i < curves.size(); ++i)
		polylines(designImage, curves[i], false, 0, lineThicknessOfLevel[2], LINE_AA);
	polylines(designImage, mesialCurve, false, 0, lineThicknessOfLevel[2], LINE_AA);
	polylines(designImage, distalCurve, false, 0, lineThicknessOfLevel[2], LINE_AA);
}

RingClasp::RingClasp(const vector<Position>& positions, const Material& material) : Rpd(positions), RpdWithMaterial(material) {}

RingClasp* RingClasp::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midResourceGetProperty, const jmethodID& midStatementGetProperty, const jobject& dpClaspMaterial, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions;
	Material claspMaterial;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed);
	queryMaterial(env, midGetInt, midResourceGetProperty, dpClaspMaterial, individual, claspMaterial);
	return new RingClasp(positions, claspMaterial);
}

void RingClasp::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	OcclusalRest(positions_, MESIAL).draw(designImage, teeth);
	if (material_ == CAST)
		OcclusalRest(positions_, DISTAL).draw(designImage, teeth);
	auto isUpper = positions_[0].zone < nZones / 2;
	polylines(designImage, getTooth(teeth, positions_[0]).getCurve(isUpper ? 60 : 0, isUpper ? 0 : 300), false, 0, lineThicknessOfLevel[1 + (material_ == CAST) ? 1 : 0], LINE_AA);
}

Rpa::Rpa(const vector<Position>& positions, const Material& material) : Rpd(positions), RpdWithMaterial(material) {}

Rpa* Rpa::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midResourceGetProperty, const jmethodID& midStatementGetProperty, const jobject& dpClaspMaterial, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions;
	Material claspMaterial;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed);
	queryMaterial(env, midGetInt, midResourceGetProperty, dpClaspMaterial, individual, claspMaterial);
	return new Rpa(positions, claspMaterial);
}

void Rpa::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	OcclusalRest(positions_, MESIAL).draw(designImage, teeth);
	GuidingPlate(positions_).draw(designImage, teeth);
	HalfClasp(positions_, material_, MESIAL, HalfClasp::BUCCAL).draw(designImage, teeth);
}

Rpi::Rpi(const vector<Position>& positions) : Rpd(positions) {}

Rpi* Rpi::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midStatementGetProperty, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed);
	return new Rpi(positions);
}

void Rpi::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	OcclusalRest(positions_, MESIAL).draw(designImage, teeth);
	GuidingPlate(positions_).draw(designImage, teeth);
	IBar(positions_).draw(designImage, teeth);
}

WwClasp::WwClasp(const vector<Position>& positions, const Direction& direction) : RpdWithDirection(direction), RpdWithLingualClaspArms(positions, WROUGHT_WIRE, direction) {}

WwClasp* WwClasp::createFromIndividual(JNIEnv* const& env, const jmethodID& midGetInt, const jmethodID& midHasNext, const jmethodID& midListProperties, const jmethodID& midNext, const jmethodID& midResourceGetProperty, const jmethodID& midStatementGetProperty, const jobject& dpClaspTipDirection, const jobject& dpToothZone, const jobject& dpToothOrdinal, const jobject& opComponentPosition, const jobject& individual, bool isEighthToothUsed[nZones]) {
	vector<Position> positions;
	Direction claspTipDirection;
	queryPositions(env, midGetInt, midHasNext, midListProperties, midNext, midStatementGetProperty, dpToothZone, dpToothOrdinal, opComponentPosition, individual, positions, isEighthToothUsed);
	queryDirection(env, midGetInt, midResourceGetProperty, dpClaspTipDirection, individual, claspTipDirection);
	return new WwClasp(positions, claspTipDirection);
}

void WwClasp::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	OcclusalRest(positions_, direction_ == MESIAL ? DISTAL : MESIAL).draw(designImage, teeth);
	RpdWithLingualClaspArms::draw(designImage, teeth);
	HalfClasp(positions_, WROUGHT_WIRE, direction_, HalfClasp::BUCCAL).draw(designImage, teeth);
}

GuidingPlate::GuidingPlate(const vector<Position>& positions) : Rpd(positions) {}

void GuidingPlate::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	auto& tooth = getTooth(teeth, positions_[0]);
	auto& centroid = tooth.getCentroid();
	auto point = roundToInt(centroid + (static_cast<Point2f>(tooth.getAnglePoint(180)) - centroid) * 1.1);
	auto direction = roundToInt(computeNormalDirection(point) * tooth.getRadius() * 2 / 3);
	line(designImage, point, point + direction, 0, lineThicknessOfLevel[2], LINE_AA);
	line(designImage, point, point - direction, 0, lineThicknessOfLevel[2], LINE_AA);
}

HalfClasp::HalfClasp(const vector<Position>& positions, const Material& material, const Direction& direction, const Side& side) : Rpd(positions), RpdWithMaterial(material), RpdWithDirection(direction), side_(side) {}

HalfClasp::HalfClasp(const Position& position, const Material& material, const Direction& direction, const Side& side) : HalfClasp(vector<Position>{position}, material, direction, side) {}

void HalfClasp::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	auto& tooth = getTooth(teeth, positions_[0]);
	vector<Point> curve;
	switch ((direction_ == DISTAL) * 2 + (side_ == LINGUAL)) {
		case 0b00:
			curve = tooth.getCurve(60, 180);
			break;
		case 0b01:
			curve = tooth.getCurve(180, 300);
			break;
		case 0b10:
			curve = tooth.getCurve(0, 120);
			break;
		case 0b11:
			curve = tooth.getCurve(240, 0);
			break;
		default: ;
	}
	polylines(designImage, curve, false, 0, lineThicknessOfLevel[1 + (material_ == CAST) ? 1 : 0], LINE_AA);
}

IBar::IBar(const vector<Position>& positions) : Rpd(positions) {}

void IBar::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const {
	auto& tooth = getTooth(teeth, positions_[0]);
	auto a = tooth.getRadius() * 1.5;
	Point2f point1 = tooth.getAnglePoint(75), point2 = tooth.getAnglePoint(165);
	auto center = (point1 + point2) / 2;
	auto direction = computeNormalDirection(center);
	auto r = point1 - center;
	auto rou = norm(r);
	auto sinTheta = direction.cross(r) / rou;
	auto b = rou * abs(sinTheta) / sqrt(1 - pow(rou / a, 2) * (1 - pow(sinTheta, 2)));
	auto inclination = atan2(direction.y, direction.x);
	auto t = radianToDegree(asin(rotate(r, -inclination).y / b));
	inclination = radianToDegree(inclination);
	if (t > 0)
		t -= 180;
	ellipse(designImage, center, roundToInt(Size(a, b)), inclination, t, t + 180, 0, lineThicknessOfLevel[2], LINE_AA);
}

Plating::Plating(const vector<Position>& positions) : Rpd(positions) {}

void Plating::draw(const Mat& designImage, const vector<Tooth> teeth[nZones]) const { polylines(designImage, getTooth(teeth, positions_[0]).getCurve(180, 0), false, 0, lineThicknessOfLevel[2], LINE_AA); }

/*
 * XYservo.h
 *
 *  Created on: Sep 28, 2017
 *      Author: Nick
 */

#ifndef XYSERVO_H_
#define XYSERVO_H_

namespace syslog {

class XYservo {
public:
	XYservo();
	void moveServo();
	virtual ~XYservo();
};

} /* namespace syslog */

#endif /* XYSERVO_H_ */

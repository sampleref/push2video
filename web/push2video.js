var videosrc;
var avSendRecv;
var avSendRecvSelf;
var audioRecv;
var peerConnectionAVSend;
var peerConnectionAVRecv;
var peerConnectionVideoSend;
var peerConnectionVideoRecv;
var peerConnectionAudioSend;
var peerConnectionAudioRecv;
var peerConnectionConfig = { 'iceServers': [{ 'url': 'stun:stun.services.mozilla.com' }, { 'url': 'stun:stun.l.google.com:19302' }]};

var channelIdVar = "AUDI0_001_CHANNEL";
var groupIdVar = "DEFAULT_AV_GROUP_100";
var peerIdVar;
var streamVar;
var videoLock = true;

navigator.getUserMedia = navigator.getUserMedia || navigator.mozGetUserMedia || navigator.webkitGetUserMedia;
window.RTCPeerConnection = window.RTCPeerConnection || window.mozRTCPeerConnection || window.webkitRTCPeerConnection;
window.RTCIceCandidate = window.RTCIceCandidate || window.mozRTCIceCandidate || window.webkitRTCIceCandidate;
window.RTCSessionDescription = window.RTCSessionDescription || window.mozRTCSessionDescription || window.webkitRTCSessionDescription;

function getUserMediaSuccess(stream) {
    console.log('Local media read successful');
	streamVar = stream;
}

function getUserMediaError(error) {
    alert('Cannot read user media. This application cannot function!');
    console.log(error);
}

//Once page ready create websocket server connection and keep ready, constraints
// to be kept false for audio and video as no media is sent from browser. Its receive only
//Add callbacks for onmessage, onopen, onerror
function pageReady() {
    videosrc = document.getElementById("videoId");
	audioRecv = document.getElementById("audioRecv");
	avSendRecv = document.getElementById("videoSendRecv");
	avSendRecvSelf = document.getElementById("videoSendRecvSelf");
    serverConnection = new WebSocket('wss://' + window.location.hostname + ':8995/signalling');
	document.getElementById("ws_msg").innerHTML  = "Please open url: <a href='https://"
													+ window.location.hostname
													+ ":8995/signalling' target='_blank'>https://"
													+ window.location.hostname
													+ ":8995/signalling</a> in new tab and "
													+ "validate certificate if <b>'Peer Id'</b> below is not assigned and Refresh(F5)";
    serverConnection.onmessage = gotMessageFromServer;
    serverConnection.onopen = ServerOpen;
    serverConnection.onerror = ServerError;
    var constraints = {
        video: true,
        audio: true,
    };

    if (navigator.getUserMedia) {
        navigator.getUserMedia(constraints, getUserMediaSuccess, getUserMediaError);
    } else {
        alert('Your browser does not support getUserMedia API');
    }
	//Start stats monitor
	statsInterval = window.setInterval(startConnectionStats, 1000);
}


function ServerOpen() {
    console.log("Server open");
}

function ServerError(error) {
    console.log("Server error ", error);
}

function toggleStartVideo(enable) {
    if(enable){
        document.getElementById("startVideo").disabled = false;
    }else{
        document.getElementById("startVideo").disabled = true;
    }
}

function toggleStopVideo(enable) {
    if(enable){
        document.getElementById("stopVideo").disabled = false;
    }else{
        document.getElementById("stopVideo").disabled = true;
    }
}

function setStatusText(colorVal, textVal) {
	document.getElementById("sendrecvstatus").innerHTML  = '<b><span style="color: ' + colorVal +'";>' + textVal + '</span></b>';
}

function clearConnectionStats() {
	document.getElementById("audio_recv_stats").innerHTML  = '';
	document.getElementById("video_recv_stats").innerHTML  = '';
	document.getElementById("audio_send_stats").innerHTML  = '';
	document.getElementById("video_send_stats").innerHTML  = '';
}

function startConnectionStats() {
  if(peerConnectionAVSend != null){
		peerConnectionAVSend.getStats(null).then(stats => {
			var statsOutputVideo = "";
			var statsOutputAudio = "";
			stats.forEach(report => {
				if (report.type === "outbound-rtp" && report.kind === "video") {
					Object.keys(report).forEach(statName => {
						statsOutputVideo += `<strong>${statName}:</strong> ${report[statName]}<br>\n`;
					});
				}
				if (report.type === "outbound-rtp" && report.kind === "audio") {
					Object.keys(report).forEach(statName => {
						statsOutputAudio += `<strong>${statName}:</strong> ${report[statName]}<br>\n`;
					});
				}
			});
			document.getElementById("audio_send_stats").innerHTML  = statsOutputAudio;
			document.getElementById("video_send_stats").innerHTML  = statsOutputVideo;
	   });
  }
  if(peerConnectionAVRecv != null){
		peerConnectionAVRecv.getStats(null).then(stats => {
			var statsOutputVideo = "";
			var statsOutputAudio = "";
			stats.forEach(report => {
				if (report.type === "inbound-rtp" && report.kind === "video") {
					Object.keys(report).forEach(statName => {
						statsOutputVideo += `<strong>${statName}:</strong> ${report[statName]}<br>\n`;
					});
				}
				if (report.type === "inbound-rtp" && report.kind === "audio") {
					Object.keys(report).forEach(statName => {
						statsOutputAudio += `<strong>${statName}:</strong> ${report[statName]}<br>\n`;
					});
				}
			});
			document.getElementById("audio_recv_stats").innerHTML  = statsOutputAudio;
			document.getElementById("video_recv_stats").innerHTML  = statsOutputVideo;
	   });
  }
}

function joinSendRecv(){
	var offerConstraintsSend = {
        "optional": [
          { "OfferToReceiveAudio": "false" },
          { "OfferToReceiveVideo": "false" },
        ]
    };
	var offerConstraintsRecv = {
        "optional": [
          { "OfferToReceiveAudio": "true" },
          { "OfferToReceiveVideo": "false" },
        ]
    };
	// Create peerConnection and attach onicecandidate, ontrack callbacks
	try {
		peerConnectionAVSend = new RTCPeerConnection(peerConnectionConfig);
		peerConnectionAVSend.addEventListener("iceconnectionstatechange", ev => {
		  console.log('AV Send ice connection state ' + peerConnectionAVSend.iceConnectionState);
		}, false);
	} catch(err) {
		console.error("Error creating peerConnectionAVSend/Recv: " + err);
	}
	avSendRecvSelf.srcObject = streamVar;
	streamVar.getTracks().forEach(track => {
		if(track.kind == 'audio'){
			console.log('Adding audio');
			peerConnectionAVSend.addTrack(track, streamVar);
		}
		if(track.kind == 'video'){
			console.log('Adding video');
			peerConnectionAVSend.addTrack(track, streamVar);
		}
	});
	avSendRecvSelf.srcObject.getTracks().forEach(track => {
		if(track.kind == 'audio'){
			console.log('Disabling audio');
			track.enabled = false;
		}
		if(track.kind == 'video'){
			console.log('Disabling video');
			track.enabled = false;
		}
	});

    peerConnectionAVSend.onicecandidate = gotIceCandidateAVSend;
    peerConnectionAVSend.createOffer(gotDescriptionAVSend, createOfferError, offerConstraintsSend);
}

function createGroupAVReceiver(){
	peerConnectionAVRecv = new RTCPeerConnection(peerConnectionConfig);
	peerConnectionAVRecv.addEventListener("iceconnectionstatechange", ev => {
		  console.log('AV Recv ice connection state ' + peerConnectionAVRecv.iceConnectionState);
		}, false);
	peerConnectionAVRecv.onicecandidate = gotIceCandidateAVRecv;
	peerConnectionAVRecv.ontrack = gotRemoteAVRecvStream;
}

function quitSendRecv(){
	console.log("Closing quitSend/Recv");
    if(peerConnectionAVSend == null){
        alert('peerConnectionAVSend/Recv Already Stopped!');
        return;
    }
    peerConnectionAVSend.close();
	peerConnectionAVRecv.close();
	setStatusText('green', '');
	clearConnectionStats();
    peerConnectionAVSend = null;
	peerConnectionAVRecv = null;
    msg = {
		peerId: peerIdVar,
		channelId: channelIdVar,
		groupUeMessage: {
			ueResetRequest: {
				ueId: peerIdVar,
				groupId: groupIdVar
			}
		}
	};
    serverConnection.send(JSON.stringify(msg));
    console.debug("Sent peerConnectionAVSend/Recv reset");
	avSendRecvSelf.srcObject = null;
	avSendRecv.srcObject  = null;
}

function startSendRecv(){
	avSendRecvSelf.srcObject.getTracks().forEach(track => {
		if(track.kind == 'audio'){
			console.log('Enabling audio');
			track.enabled = true;
		}
		if(track.kind == 'video'){
			console.log('Enabling video');
			track.enabled = true;
		}
	});
	msg = {
		peerId: peerIdVar,
		channelId: channelIdVar,
		groupUeMessage: {
			ueFloorControlRequest: {
				ueId: peerIdVar,
				groupId: groupIdVar,
				action: "ACQUIRE",
				ueMediaDirection: {
					direction: "SENDER"
				}
			}
		}
	};
    serverConnection.send(JSON.stringify(msg));
    console.debug("Sent peerConnectionAVSend/Recv Aquire");
}

function stopSendRecv(){
	msg = {
		peerId: peerIdVar,
		channelId: channelIdVar,
		groupUeMessage: {
			ueFloorControlRequest: {
				ueId: peerIdVar,
				groupId: groupIdVar,
				action: "RELEASE",
				ueMediaDirection: {
					direction: "SENDER"
				}
			}
		}
	};
    serverConnection.send(JSON.stringify(msg));
    console.debug("Sent peerConnectionAVSend/Recv Release");
	avSendRecvSelf.srcObject.getTracks().forEach(track => {
		if(track.kind == 'audio'){
			console.log('Disabling audio');
			track.enabled = false;
		}
		if(track.kind == 'video'){
			console.log('Disabling video');
			track.enabled = false;
		}
	});
}

//Once on start create peerConnection with callbacks and then send ADD_PEER
//Once sent ADD_PEER, if device is PLAYING NDN SERVER will trigger the call as caller and browser is callee to answer else error message is sent
//In an order first SDP negotiation happens and then ICE negotiation it triggered
function startVideo(start) {
    if(!peerIdVar){
        alert('Peer Id not assigned yet!')
        return;
    }
    if(!start){
        msg = {
            peerId: peerIdVar,
            channelId: channelIdVar,
            channelStatusMessage: {
                status: 'VIDEO_UNLOCKED'
            }
        };
        serverConnection.send(JSON.stringify(msg));
        console.debug("Sent video lock request");
        return;
    }
    //Only Send Video
    var offerConstraints = {
        "optional": [
          { "OfferToReceiveAudio": "false" },
          { "OfferToReceiveVideo": "false" },
        ]
    };

    if(peerConnectionVideoSend != null){
        alert('Peer connection not null ! Stop and try again')
        return;
    }
	clearConnectionStats();
    // Create peerConnection and attach onicecandidate, ontrack callbacks
	try {
		peerConnectionVideoSend = new RTCPeerConnection(peerConnectionConfig);
		peerConnectionVideoSend.addEventListener("iceconnectionstatechange", ev => {
		  console.log('Send video ice connection state ' + peerConnectionVideoSend.iceConnectionState);
		  if('connected' == peerConnectionVideoSend.iceConnectionState) {
			setStatusText('green', 'Sending... ==>>==>>==>>');
		  }
		}, false);
	} catch(err) {
		console.error("Error creating peerConnectionVideoSend: " + err);
	}
	videosrc.srcObject = streamVar;
	streamVar.getTracks().forEach(track => {
		if(track.kind == 'audio'){
			console.log('Adding audio');
			peerConnectionVideoSend.addTrack(track, streamVar);
		}
		if(track.kind == 'video'){
			console.log('Adding video');
			peerConnectionVideoSend.addTrack(track, streamVar);
		}
	});

    peerConnectionVideoSend.onicecandidate = gotIceCandidateVideoSend;
    peerConnectionVideoSend.createOffer(gotDescriptionVideoSend, createOfferError, offerConstraints);
}

function stopVideo() {
    if(!peerIdVar){
        alert('Peer Id not assigned yet!')
        return;
    }
    console.log("Closing peerConnectionVideoSend");
    if(peerConnectionVideoSend == null){
        alert('Already Stopped!');
        return;
    }
    peerConnectionVideoSend.close();
	setStatusText('green', '');
	clearConnectionStats();
    peerConnectionVideoSend = null;
    msg = {
        peerId: peerIdVar,
        channelId: channelIdVar,
        peerStatusMessage: {
            status: 'VIDEO_RESET'
        }
    };
    serverConnection.send(JSON.stringify(msg));
    console.debug("Sent video reset");
	videosrc.srcObject = null;
}

function joinAudio(){
    if(!peerIdVar){
        alert('Peer Id not assigned yet!')
        return;
    }
	//Only Send audio
	var offerConstraints = {
		"optional": [
		  { "OfferToReceiveAudio": "false" },
		  { "OfferToReceiveVideo": "false" },
		]
	};
	// Create peerConnection and attach onicecandidate, ontrack callbacks
    peerConnectionAudioSend = new RTCPeerConnection(peerConnectionConfig);
	streamVar.getTracks().forEach(track => {
		if(track.kind == 'audio'){
			peerConnectionAudioSend.addTrack(track, streamVar);
		}
	});
    peerConnectionAudioSend.onicecandidate = gotIceCandidateAudioSend;
	peerConnectionAudioSend.createOffer(gotDescriptionAudioSend, createOfferError, offerConstraints);
    //Receive Audio
	peerConnectionAudioRecv = new RTCPeerConnection(peerConnectionConfig);
    peerConnectionAudioRecv.onicecandidate = gotIceCandidateAudioRecv;
    peerConnectionAudioRecv.ontrack = gotRemoteAudioStream;
}

function quitAudio(){
    if(!peerIdVar){
        alert('Peer Id not assigned yet!')
        return;
    }
    if(peerConnectionAudioSend != null){
        peerConnectionAudioSend.close();
        peerConnectionAudioSend = null;
    }
    if(peerConnectionAudioRecv != null){
        peerConnectionAudioRecv.close();
        peerConnectionAudioRecv = null;
    }
    msg = {
        peerId: peerIdVar,
        channelId: channelIdVar,
        peerStatusMessage: {
            status: 'AUDIO_RESET'
        }
    };
    serverConnection.send(JSON.stringify(msg));
    console.debug("Sent audio reset");
	videosrc.srcObject = null;
}

function startRecvVideo(){
	clearConnectionStats();
    peerConnectionVideoRecv = new RTCPeerConnection(peerConnectionConfig);
	peerConnectionVideoRecv.addEventListener("iceconnectionstatechange", ev => {
	  console.log('Receive video ice connection state ' + peerConnectionVideoRecv.iceConnectionState);
	  if('connected' == peerConnectionVideoRecv.iceConnectionState) {
		setStatusText('red', 'Receiving... <<==<<==<<==');
	  }
	  if('disconnected' == peerConnectionVideoRecv.iceConnectionState) {
		console.log('Closing Video Reception!');
		setStatusText('green', '');
		clearConnectionStats();
		peerConnectionVideoRecv.close();
		peerConnectionVideoRecv = null;
	  }
	}, false);
    peerConnectionVideoRecv.onicecandidate = gotIceCandidateVideoRecv;
    peerConnectionVideoRecv.ontrack = gotRemoteVideoStream;
}

function gotRemoteVideoStream(event) {
    console.log('got remote video stream ', event);
    videosrc.srcObject  = event.streams[0];
}

function gotRemoteAVRecvStream(event) {
    console.log('got remote av recv stream ', event);
    avSendRecv.srcObject  = event.streams[0];
}

function gotRemoteAudioStream(event) {
    console.log('got remote audio stream ', event);
    audioRecv.srcObject  = event.streams[0];
}

function gotDescriptionAVRecvAnswer(description) {
    console.log('got gotDescriptionAVRecvAnswer ', description);
    peerConnectionAVRecv.setLocalDescription(description, function() {
        msg = {
			peerId: peerIdVar,
			channelId: channelIdVar,
			groupUeMessage: {
				sdpAnswerRequest: {
					sdp: description.sdp,
					ueId: peerIdVar,
					groupId: groupIdVar,
					ueMediaDirection: {
						direction: "RECEIVER"
					}
				}
			}
		};
        serverConnection.send(JSON.stringify(msg));
        console.debug("Sent gotDescriptionAVRecvAnswer sdp");
    }, function() { console.log('set gotDescriptionAVRecvAnswer description error') });
}

function gotDescriptionVideoRecv(description) {
    console.log('got description video recv ', description);
    peerConnectionVideoRecv.setLocalDescription(description, function() {
        msg = {
            peerId: peerIdVar,
            channelId: channelIdVar,
            sdpMessage: {
                sdp: description.sdp,
                type: "answer",
                direction: "RECEIVER",
                endpoint: "CLIENT",
                mediaType: "VIDEO"
            }
        };
        serverConnection.send(JSON.stringify(msg));
        console.debug("Sent gotDescriptionVideoRecv sdp");
    }, function() { console.log('set gotDescriptionVideoRecv description error') });
}

function gotDescriptionAVSend(description){
	console.log('gotDescriptionAVSend ', description);
    peerConnectionAVSend.setLocalDescription(description);
	msg = {
		peerId: peerIdVar,
		channelId: channelIdVar,
		groupUeMessage: {
			sdpOfferRequest: {
				sdp: description.sdp,
				ueId: peerIdVar,
				groupId: groupIdVar,
				ueMediaDirection: {
					direction: "SENDER"
				}
			}
		}
	};
	serverConnection.send(JSON.stringify(msg));
	console.debug("Sent gotDescriptionAVSendRecv sdp");
}

function gotDescriptionAVRecv(description){
	console.log('gotDescriptionAVRecv ', description);
    peerConnectionAVRecv.setLocalDescription(description);
	msg = {
		peerId: peerIdVar,
		channelId: channelIdVar,
		groupUeMessage: {
			sdpOfferRequest: {
				sdp: description.sdp,
				ueId: peerIdVar,
				groupId: groupIdVar,
				ueMediaDirection: {
					direction: "RECEIVER"
				}
			}
		}
	};
	serverConnection.send(JSON.stringify(msg));
	console.debug("Sent gotDescriptionAVSendRecv sdp");
}

function gotDescriptionVideoSend(description){
	console.log('gotDescriptionVideoSend ', description);
    peerConnectionVideoSend.setLocalDescription(description);
	msg = {
		peerId: peerIdVar,
		channelId: channelIdVar,
		sdpMessage: {
			sdp: description.sdp,
			type: "offer",
			direction: "SENDER",
			endpoint: "CLIENT",
			mediaType: "VIDEO"
		}
	};
	serverConnection.send(JSON.stringify(msg));
	console.debug("Sent gotDescriptionVideoSend sdp");
}

function gotDescriptionAudioSend(description){
	console.log('got description audio send ', description);
    peerConnectionAudioSend.setLocalDescription(description);
	msg = {
		peerId: peerIdVar,
		channelId: channelIdVar,
		sdpMessage: {
			sdp: description.sdp,
			type: "offer",
			direction: "SENDER",
			endpoint: "CLIENT",
			mediaType: "AUDIO"
		}
	};
	serverConnection.send(JSON.stringify(msg));
	console.debug("Sent gotDescriptionAudioSend sdp");
}

function gotDescriptionAudioRecv(description){
	console.log('got description audio recv ', description);
    peerConnectionAudioRecv.setLocalDescription(description, function() {
        msg = {
            peerId: peerIdVar,
			channelId: channelIdVar,
            sdpMessage: {
				sdp: description.sdp,
				type: "answer",
				direction: "RECEIVER",
				endpoint: "CLIENT",
				mediaType: "AUDIO"
            }
        };
        serverConnection.send(JSON.stringify(msg));
        console.debug("Sent gotDescriptionAudioRecv sdp");
    }, function() { console.log('set gotDescriptionAudioRecv description error') });
}

function gotIceCandidateAVSend(event) {
    console.debug("Ice Candidate gotIceCandidateAVSend: ", event);
    if (event.candidate != null) {
		msg = {
            peerId: peerIdVar,
			channelId: channelIdVar,
			groupUeMessage: {
				iceMessageRequest: {
					ice: event.candidate.candidate,
					mLineIndex: event.candidate.sdpMLineIndex,
					ueId: peerIdVar,
					groupId: groupIdVar,
					ueMediaDirection: {
						direction: "SENDER"
					}
				}
			}
        };

        console.debug(msg);
        serverConnection.send(JSON.stringify(msg));
    }
}

function gotIceCandidateAVRecv(event) {
    console.debug("Ice Candidate gotIceCandidateAVRecv: ", event);
    if (event.candidate != null) {
		msg = {
            peerId: peerIdVar,
			channelId: channelIdVar,
			groupUeMessage: {
				iceMessageRequest: {
					ice: event.candidate.candidate,
					mLineIndex: event.candidate.sdpMLineIndex,
					ueId: peerIdVar,
					groupId: groupIdVar,
					ueMediaDirection: {
						direction: "RECEIVER"
					}
				}
			}
        };

        console.debug(msg);
        serverConnection.send(JSON.stringify(msg));
    }
}

function gotIceCandidateVideoSend(event) {
    console.debug("Ice Candidate gotIceCandidateVideoSend: ", event);
    if (event.candidate != null) {
		msg = {
            peerId: peerIdVar,
			channelId: channelIdVar,
            iceMessage: {
				ice: event.candidate.candidate,
				mLineIndex: event.candidate.sdpMLineIndex,
				direction: "SENDER",
				endpoint: "CLIENT",
				mediaType: "VIDEO"
            }
        };

        console.debug(msg);
        serverConnection.send(JSON.stringify(msg));
    }
}

function gotIceCandidateAudioSend(event) {
    console.debug("Ice Candidate gotIceCandidateAudioSend: ", event);
    if (event.candidate != null) {
		msg = {
            peerId: peerIdVar,
			channelId: channelIdVar,
            iceMessage: {
				ice: event.candidate.candidate,
				mLineIndex: event.candidate.sdpMLineIndex,
				direction: "SENDER",
				endpoint: "CLIENT",
				mediaType: "AUDIO"
            }
        };
        console.debug(msg);
        serverConnection.send(JSON.stringify(msg));
    }
}

function gotIceCandidateVideoRecv(event) {
    console.debug("Ice Candidate gotIceCandidateVideoRecv: ", event);

    if (event.candidate != null) {
		msg = {
            peerId: peerIdVar,
			channelId: channelIdVar,
            iceMessage: {
				ice: event.candidate.candidate,
				mLineIndex: event.candidate.sdpMLineIndex,
				direction: "RECEIVER",
				endpoint: "CLIENT",
				mediaType: "VIDEO"
            }
        };

        console.debug(msg);
        serverConnection.send(JSON.stringify(msg));
    }
}

function gotIceCandidateAudioRecv(event) {
    console.debug("Ice Candidate gotIceCandidateAudioRecv: ", event);
    if (event.candidate != null) {
		msg = {
            peerId: peerIdVar,
			channelId: channelIdVar,
            iceMessage: {
				ice: event.candidate.candidate,
				mLineIndex: event.candidate.sdpMLineIndex,
				direction: "RECEIVER",
				endpoint: "CLIENT",
				mediaType: "AUDIO"
            }
        };
        console.debug(msg);
        serverConnection.send(JSON.stringify(msg));
    }
}

function createOfferError(error) {
    console.log(error);
}

function createAnswerError(error) {
    console.log(error);
}

function gotMessageFromServer(message) {
    console.info(" Message from server:", message);

    var signal = JSON.parse(message.data);
	if(signal.peerStatusMessage){
		if(signal.peerStatusMessage.status == 'CONNECTED'){
			console.info(" Peer connected with Peer Id:", signal.peerId);
			document.getElementById("peerId").innerHTML  = '<b>' + signal.peerId + '</b>';
			peerIdVar = signal.peerId;
		}
	}else if(signal.channelStatusMessage){
	    if(signal.channelStatusMessage.status == 'VIDEO_LOCKED'){
            console.info(" Video channel locked!");
            alert('Video is locked by other peer!');
            videoLock = true;
        }
        if(signal.channelStatusMessage.status == 'VIDEO_UNLOCKED'){
            console.info(" Video channel unlocked!");
            videoLock = false;
            startVideo(true);
        }
	}
    else if (signal.sdpMessage) {
		if(signal.sdpMessage.direction == 'SENDER' && signal.sdpMessage.mediaType == 'AUDIO'){
			sdpMsg = {
				sdp: signal.sdpMessage.sdp,
				type: signal.sdpMessage.type
			};
			peerConnectionAudioSend.setRemoteDescription(new RTCSessionDescription(sdpMsg), function() {
			});
		}
		if(signal.sdpMessage.direction == 'RECEIVER' && signal.sdpMessage.mediaType == 'AUDIO'){
			sdpMsg = {
				sdp: signal.sdpMessage.sdp,
				type: signal.sdpMessage.type
			};
			peerConnectionAudioRecv.setRemoteDescription(new RTCSessionDescription(sdpMsg), function() {
				peerConnectionAudioRecv.createAnswer(gotDescriptionAudioRecv, createAnswerError);
			});
		}
		if(signal.sdpMessage.direction == 'SENDER' && signal.sdpMessage.mediaType == 'VIDEO'){
            sdpMsg = {
                sdp: signal.sdpMessage.sdp,
                type: signal.sdpMessage.type
            };
            peerConnectionVideoSend.setRemoteDescription(new RTCSessionDescription(sdpMsg), function() {
            });
        }
        if(signal.sdpMessage.direction == 'RECEIVER' && signal.sdpMessage.mediaType == 'VIDEO'){
            startRecvVideo();
            sdpMsg = {
                sdp: signal.sdpMessage.sdp,
                type: signal.sdpMessage.type
            };
            peerConnectionVideoRecv.setRemoteDescription(new RTCSessionDescription(sdpMsg), function() {
                peerConnectionVideoRecv.createAnswer(gotDescriptionVideoRecv, createAnswerError);
            });
        }
    }
	else if (signal.iceMessage) {
		if(signal.iceMessage.direction == 'SENDER' && signal.iceMessage.mediaType == 'AUDIO'){
			iceMsg = {
				candidate: signal.iceMessage.ice,
				sdpMLineIndex: signal.iceMessage.mLineIndex
			};
			peerConnectionAudioSend.addIceCandidate(new RTCIceCandidate(iceMsg));
		}
		if(signal.iceMessage.direction == 'RECEIVER' && signal.iceMessage.mediaType == 'AUDIO'){
			iceMsg = {
				candidate: signal.iceMessage.ice,
				sdpMLineIndex: signal.iceMessage.mLineIndex
			};
			peerConnectionAudioRecv.addIceCandidate(new RTCIceCandidate(iceMsg));
		}
		if(signal.iceMessage.direction == 'SENDER' && signal.iceMessage.mediaType == 'VIDEO'){
            iceMsg = {
                candidate: signal.iceMessage.ice,
                sdpMLineIndex: signal.iceMessage.mLineIndex
            };
            peerConnectionVideoSend.addIceCandidate(new RTCIceCandidate(iceMsg));
        }
        if(signal.iceMessage.direction == 'RECEIVER' && signal.iceMessage.mediaType == 'VIDEO'){
            iceMsg = {
                candidate: signal.iceMessage.ice,
                sdpMLineIndex: signal.iceMessage.mLineIndex
            };
            peerConnectionVideoRecv.addIceCandidate(new RTCIceCandidate(iceMsg));
        }
    }
	else if (signal.groupUeMessage) {
		if(signal.groupUeMessage.sdpAnswerRequest){
			if(signal.groupUeMessage.sdpAnswerRequest.ueMediaDirection.direction == 'SENDER'){
				sdpMsg = {
					sdp: signal.groupUeMessage.sdpAnswerRequest.sdp,
					type: "answer"
				};
				peerConnectionAVSend.setRemoteDescription(new RTCSessionDescription(sdpMsg), function() {
				});
			}
			if(signal.groupUeMessage.sdpAnswerRequest.ueMediaDirection.direction == 'RECEIVER'){
				sdpMsg = {
					sdp: signal.groupUeMessage.sdpAnswerRequest.sdp,
					type: "answer"
				};
				peerConnectionAVRecv.setRemoteDescription(new RTCSessionDescription(sdpMsg), function() {
				});
			}
		}
		if(signal.groupUeMessage.sdpOfferRequest){
			createGroupAVReceiver();
			if(signal.groupUeMessage.sdpOfferRequest.ueMediaDirection.direction == 'RECEIVER'){
				sdpMsg = {
					sdp: signal.groupUeMessage.sdpOfferRequest.sdp,
					type: "offer"
				};
				peerConnectionAVRecv.setRemoteDescription(new RTCSessionDescription(sdpMsg), function() {
					peerConnectionAVRecv.createAnswer(gotDescriptionAVRecvAnswer, createAnswerError);
				});
			}
		}
		if(signal.groupUeMessage.iceMessageRequest){
			if(signal.groupUeMessage.iceMessageRequest.ueMediaDirection.direction == 'SENDER'){
				iceMsg = {
					candidate: signal.groupUeMessage.iceMessageRequest.ice,
					sdpMLineIndex: signal.groupUeMessage.iceMessageRequest.mLineIndex
				};
				peerConnectionAVSend.addIceCandidate(new RTCIceCandidate(iceMsg));
			}
			if(signal.groupUeMessage.iceMessageRequest.ueMediaDirection.direction == 'RECEIVER'){
				iceMsg = {
					candidate: signal.groupUeMessage.iceMessageRequest.ice,
					sdpMLineIndex: signal.groupUeMessage.iceMessageRequest.mLineIndex
				};
				peerConnectionAVRecv.addIceCandidate(new RTCIceCandidate(iceMsg));
			}

        }
    }
}
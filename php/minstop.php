<?php
//good test stop 101924

function get_html_site($site_address){
  $timeout = 5;
  $curl_handle=curl_init();
  curl_setopt($curl_handle,CURLOPT_URL,$site_address);
  curl_setopt($curl_handle,CURLOPT_CONNECTTIMEOUT,$timeout);#seconds
  curl_setopt($curl_handle,CURLOPT_TIMEOUT,$timeout);
  curl_setopt($curl_handle,CURLOPT_RETURNTRANSFER,1);#return output of curl_exec instead of printing it
  $buffer = curl_exec($curl_handle);
  $status = curl_getinfo($curl_handle, CURLINFO_HTTP_CODE);
  curl_close($curl_handle);
  if (empty($buffer) || $status != 200){
    return 0;
  }else{
    return $buffer;
  }
}

header("Content-Type: text/plain");

//Phil's hacked together script to parse siri into something for the sign network

if(!isset($_GET['id'])) {
	echo "END\nGET Parameter 'id' required as stop id.";
	exit();
}

$lineoutput = 5;
if(isset($_GET['lines'])) {
	if(is_numeric($_GET['lines'])){
		$lineoutput = $_GET['lines'];
	}
}

$stopid = $_GET['id'];
if(!is_numeric($stopid)){
	echo "END\nGET Parameter 'id' for stop id has to be numeric.";
	exit();
}

$url_template = "http://bustime.mta.info/api/siri/stop-monitoring.json?key=TEST&OperatorRef=MTA&StopMonitoringDetailLevel=minimum&MonitoringRef=%d";

$url = sprintf($url_template, $stopid);

$json_data = get_html_site($url);
$data = json_decode($json_data, TRUE);

$servtime = $data['Siri']['ServiceDelivery']['ResponseTimestamp'];
$servtime_epoch = strtotime($servtime);

$timeitems = array();
$stopitems = array();
foreach($data['Siri']['ServiceDelivery']['StopMonitoringDelivery'] as $msv){
	if(isset($msv['ErrorCondition'])){
		echo "END\nGET Parameter 'id' for stop $stopid doesn't exist, some error occurred.";
		exit();
	}
	foreach($msv['MonitoredStopVisit'] as $mvj ){
		$recordedtime = $mvj['RecordedAtTime'];
		$recordedtime_epoch = strtotime($recordedtime);
		if(abs($servtime_epoch - $recordedtime_epoch) > 300){
			//5 minutes old... let's just skip
			continue;
		}

		$items = $mvj['MonitoredVehicleJourney'];
		if(isset($items['MonitoredCall'])){
			$route = str_replace("-", "", $items['PublishedLineName']);
			if(isset($items['MonitoredCall']['ExpectedArrivalTime'])){
				$eta = $items['MonitoredCall']['ExpectedArrivalTime'];
				$eta_epoch = strtotime($eta);
				$diff = $eta_epoch - $recordedtime_epoch;
				if($diff > 0){
					$diff_min = ceil($diff / 60);
					array_push($timeitems, $route."@".$diff_min);
				}
			}else{
				$stops = $items['MonitoredCall']['Extensions']['Distances']['StopsFromCall'];
				array_push($stopitems, $route."#".$stops);
			}
		}
	}
}

$count = 0;
if(count($timeitems) > 0){
	foreach($timeitems as $line){
		if($lineoutput > $count){
			echo "$line\n";
			$count++;
		}
	}
}
if($lineoutput > $count){
	foreach($stopitems as $line){
		if($lineoutput > $count){
			echo "$line\n";
			$count++;
		}
	}

}

?>

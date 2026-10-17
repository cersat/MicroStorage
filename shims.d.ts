// Auto-generated. Do not edit.
declare namespace flashstore {

	//% shim=flashstore::writeRaw
	function writeRaw(key: string, value: string): number;

	//% shim=flashstore::readRaw
	function readRaw(key: string): string;

	//% shim=flashstore::hasKey
	function hasKey(key: string): number;

	//% shim=flashstore::removeKey
	function removeKey(key: string): number;

	//% shim=flashstore::clearAll
	function clearAll(): number;
}


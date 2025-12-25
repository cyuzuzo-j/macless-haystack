import 'dart:collection';
import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter/foundation.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_settings_screens/flutter_settings_screens.dart';
import 'package:macless_haystack/findMy/models.dart';
import 'package:macless_haystack/findMy/reports_fetcher.dart';
import 'package:logger/logger.dart';
import 'package:pointycastle/export.dart';

// ignore: implementation_imports
import 'package:pointycastle/src/platform_check/platform_check.dart';

// ignore: implementation_imports
import 'package:pointycastle/src/utils.dart' as pc_utils;

import '../preferences/user_preferences_model.dart';

class FindMyController {
  static const _storage = FlutterSecureStorage();
  static final ECCurve_secp224r1 _curveParams = ECCurve_secp224r1();
  static final HashMap _keyCache = HashMap();
  // NIST P-224 order (curve order used for scalar modulo)
  static final BigInt _p224Order = BigInt.parse(
      'ffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3d',
      radix: 16);

  static final logger = Logger(
    printer: PrettyPrinter(methodCount: 0),
  );

  /// Starts a new, fetches and decrypts all location reports
  /// for the given [FindMyKeyPair].
  /// Returns a list of [FindMyLocationReport]'s.
  static Future<List<FindMyLocationReport>> computeResults(
      List<FindMyKeyPair> keyPairs, String? url) async {
    for (var kp in keyPairs) {
      logger.i('Loading private key for ${kp.hashedPublicKey}');
      await _loadPrivateKey(kp);
    }

    Map map = <String, Object>{};
    map['keyPair'] = keyPairs;
    if (url?.isEmpty ?? true) {
      url = 'http://localhost:6176';
    }

    map['url'] = url;
    map['daysToFetch'] =
        Settings.getValue<int>(numberOfDaysToFetch, defaultValue: 7)!;
    map['user'] = Settings.getValue<String>(endpointUser, defaultValue: '')!;
    map['pass'] = Settings.getValue<String>(endpointPass, defaultValue: '')!;
    return compute(_getListedReportResults, map);
  }

  /// Fetches and decrypts the location reports for the given
  /// [FindMyKeyPair] from apples FindMy Network.
  /// Returns a list of [FindMyLocationReport].
  static Future<List<FindMyLocationReport>> _getListedReportResults(
      Map map) async {
    List<FindMyLocationReport> results = <FindMyLocationReport>[];
    List<FindMyKeyPair> keyPairs = map['keyPair'];
    var url = map['url'];
    int daysToFetch = map['daysToFetch'];
    Map<String, FindMyKeyPair> hashedKeyKeyPairsMap = {
      for (var e in keyPairs) e.getHashedAdvertisementKey(): e
    };

    List jsonResults = await ReportsFetcher.fetchLocationReports(
        hashedKeyKeyPairsMap.keys, daysToFetch, url, map['user'], map['pass']);
    FindMyLocationReport? latest;
    DateTime latestDate = DateTime.fromMicrosecondsSinceEpoch(0);
    for (var result in jsonResults) {
      DateTime currentDate =
          DateTime.fromMillisecondsSinceEpoch(result['datePublished']);
      FindMyKeyPair keyPair =
          hashedKeyKeyPairsMap[result['id']] as FindMyKeyPair;
      var currentReport = FindMyLocationReport.decrypted(
        result,
        keyPair.getBase64PrivateKey(),
        keyPair.getHashedAdvertisementKey(),
      );
      if (currentDate.isAfter(latestDate)) {
        latest = currentReport;
        latestDate = currentDate;
      }
      results.add(currentReport);
    }
    if (latest != null) {
      await latest.decrypt();
    }
    return results;
  }

  /// Loads the private key from the local cache or secure storage and adds it
  /// to the given [FindMyKeyPair].
  static Future<void> _loadPrivateKey(FindMyKeyPair keyPair) async {
    String? privateKey;
    if (!_keyCache.containsKey(keyPair.hashedPublicKey)) {
      privateKey = await _storage.read(key: keyPair.hashedPublicKey);
      // Fallback: If not found in storage (e.g. rolling keys), use the key from the object itself.
      privateKey ??= keyPair.getBase64PrivateKey();

      _keyCache[keyPair.hashedPublicKey] = privateKey;
    } else {
      privateKey = _keyCache[keyPair.hashedPublicKey];
    }
    keyPair.privateKeyBase64 = privateKey!;
  }

  /// Derives an [ECPublicKey] from a given [ECPrivateKey] on the given curve.
  static ECPublicKey _derivePublicKey(ECPrivateKey privateKey) {
    final pk = _curveParams.G * privateKey.d;
    final publicKey = ECPublicKey(pk, _curveParams);
    return publicKey;
  }

  /// Returns the to the base64 encoded given hashed public key
  /// corresponding [FindMyKeyPair] from the local [FlutterSecureStorage].
  static Future<FindMyKeyPair> getKeyPair(String base64HashedPublicKey) async {
    final privateKeyBase64 = await _storage.read(key: base64HashedPublicKey);

    ECPrivateKey privateKey = ECPrivateKey(
        pc_utils.decodeBigIntWithSign(1, base64Decode(privateKeyBase64!)),
        _curveParams);
    ECPublicKey publicKey = _derivePublicKey(privateKey);

    return FindMyKeyPair(
        publicKey, base64HashedPublicKey, privateKey, DateTime.now(), -1);
  }

  /// Imports a base64 encoded private key to the local [FlutterSecureStorage].
  /// Returns a [FindMyKeyPair] containing the corresponding [ECPublicKey].
  static Future<FindMyKeyPair> importKeyPair(String privateKeyBase64) async {
    final privateKeyBytes = base64Decode(privateKeyBase64);
    final ECPrivateKey privateKey = ECPrivateKey(
        pc_utils.decodeBigIntWithSign(1, privateKeyBytes), _curveParams);
    final ECPublicKey publicKey = _derivePublicKey(privateKey);
    final hashedPublicKey = getHashedPublicKey(publicKey: publicKey);
    final keyPair = FindMyKeyPair(
        publicKey, hashedPublicKey, privateKey, DateTime.now(), -1);

    await _storage.write(
        key: hashedPublicKey, value: keyPair.getBase64PrivateKey());

    return keyPair;
  }

  /// Generates a [ECCurve_secp224r1] keypair.
  /// Returns the newly generated keypair as a [FindMyKeyPair] object.
  static Future<FindMyKeyPair> generateKeyPair() async {
    final ecCurve = ECCurve_secp224r1();
    final secureRandom = SecureRandom('Fortuna')
      ..seed(
          KeyParameter(Platform.instance.platformEntropySource().getBytes(32)));
    ECKeyGenerator keyGen = ECKeyGenerator()
      ..init(ParametersWithRandom(
          ECKeyGeneratorParameters(ecCurve), secureRandom));

    final newKeyPair = keyGen.generateKeyPair();
    final ECPublicKey publicKey = newKeyPair.publicKey;
    final ECPrivateKey privateKey = newKeyPair.privateKey;
    final hashedKey = getHashedPublicKey(publicKey: publicKey);
    final keyPair =
        FindMyKeyPair(publicKey, hashedKey, privateKey, DateTime.now(), -1);
    await _storage.write(key: hashedKey, value: keyPair.getBase64PrivateKey());

    return keyPair;
  }

  /// Returns hashed, base64 encoded public key for given [publicKeyBytes]
  /// or for an [ECPublicKey] object [publicKey], if [publicKeyBytes] equals null.
  /// Returns the base64 encoded hashed public key as a [String].
  static String getHashedPublicKey(
      {Uint8List? publicKeyBytes, ECPublicKey? publicKey}) {
    Uint8List pkBytes;
    if (publicKeyBytes != null) {
      pkBytes = publicKeyBytes;
    } else {
      final encoded = publicKey!.Q!.getEncoded(true);
      pkBytes = encoded.sublist(1);
    }

    final shaDigest = SHA256Digest();
    shaDigest.update(pkBytes, 0, pkBytes.lengthInBytes);
    Uint8List out = Uint8List(shaDigest.digestSize);
    shaDigest.doFinal(out, 0);
    return base64Encode(out);
  }

  /// ANSI X9.63 KDF using SHA-256.
  /// Produces `length` bytes from `inputKey` and `sharedInfo`.
  static Uint8List ansiX963Kdf(
      Uint8List inputKey, String sharedInfo, int length) {
    final List<int> out = [];
    int counter = 1;
    final sharedBytes = Uint8List.fromList(utf8.encode(sharedInfo));

    while (out.length < length) {
      final d = SHA256Digest();

      // counter as 4-byte big-endian
      final counterBytes = Uint8List(4);
      final bv = ByteData.view(counterBytes.buffer);
      bv.setUint32(0, counter, Endian.big);

      d.update(inputKey, 0, inputKey.length);
      d.update(counterBytes, 0, counterBytes.length);
      d.update(sharedBytes, 0, sharedBytes.length);

      final tmp = Uint8List(d.digestSize);
      d.doFinal(tmp, 0);
      out.addAll(tmp);
      counter += 1;
    }

    return Uint8List.fromList(out.sublist(0, length));
  }

  /// Derives the next symmetric key and the rolling advertisement public key
  /// (28 byte X-coordinate) from the given master private scalar and the
  /// current symmetric key.
  ///
  /// Returns a tuple: (nextSymmetricKey, rollingAdvertisementKeyBytes)
  static Map<String, dynamic> deriveNextKeys(
      BigInt masterPrivateInt, Uint8List currentSymKey,
      {bool verbose = false}) {
    // 1) Ratchet the symmetric key
    final nextSymKey = ansiX963Kdf(currentSymKey, 'update', 32);

    // 2) Derive anti-tracking scalars u and v from SK_new
    final diversify = ansiX963Kdf(nextSymKey, 'diversify', 72);
    final uBytes = diversify.sublist(0, 36);
    final vBytes = diversify.sublist(36);

    final u = pc_utils.decodeBigIntWithSign(1, uBytes);
    final v = pc_utils.decodeBigIntWithSign(1, vBytes);

    // 3) Compute rolling private scalar: d_i = (d_0 * u + v) mod n
    BigInt rollingPrivate = (masterPrivateInt * u + v) % _p224Order;

    // 4) Create ECPrivateKey and derive public key
    final ECPrivateKey priv = ECPrivateKey(rollingPrivate, _curveParams);
    final ECPublicKey pub = _derivePublicKey(priv);

    // Use compressed encoding and drop prefix byte to get 28-byte X
    final Uint8List encoded = pub.Q!.getEncoded(true);
    final Uint8List advKey = encoded.sublist(1);

    if (verbose) {
      logger.i(
          'deriveNextKeys: nextSymKey=${base64Encode(nextSymKey)} adv=${base64Encode(advKey)}');
    }

    return {
      'nextSymKey': nextSymKey,
      'advKey': advKey,
      'privateKey': priv,
      'publicKey': pub,
    };
  }

  /// Given base64-encoded master private key and base64-encoded symmetric key
  /// this derives `count` successive rolling keys.
  ///
  /// The method ratchets the symmetric key before deriving each advertisement
  /// key (matching the broadcaster logic where SK is updated then used).
  ///
  /// Returns a Map contains 'keys' (List<FindMyKeyPair>) and 'nextSymmetricKey' (String base64).
  static Future<Map<String, dynamic>> deriveRollingKeys(
      String masterPrivBase64, String symKeyBase64, int count) async {
    // Run computation in isolate to avoid freezing UI for 2000+ keys
    Map<String, dynamic> result = await compute(_deriveRollingKeysWorker, {
      'masterPrivBase64': masterPrivBase64,
      'symKeyBase64': symKeyBase64,
      'count': count
    });

    List<dynamic> rawKeys = result['keys'];
    List<FindMyKeyPair> out =
        rawKeys.map((k) => _reconstructKeyPair(k)).toList();

    return {'keys': out, 'nextSymmetricKey': result['nextSymmetricKey']};
  }

  static FindMyKeyPair _reconstructKeyPair(Map<String, dynamic> data) {
    BigInt privateKeyInt = BigInt.parse(data['privateKeyInt']);
    ECPrivateKey privateKey = ECPrivateKey(privateKeyInt, _curveParams);

    // Use the pre-calculated public key bytes to reconstruct the Point directly.
    // This avoids the expensive G * d multiplication on the main thread.
    Uint8List pubBytes = data['publicKeyBytes'];
    ECPoint Q = _curveParams.curve.decodePoint(pubBytes)!;
    ECPublicKey publicKey = ECPublicKey(Q, _curveParams);

    return FindMyKeyPair(
        publicKey, data['hashedPublicKey'], privateKey, DateTime.now(), -1);
  }
}

/// Worker function that runs in a separate isolate.
/// Returns List of Maps to avoid passing complex objects.
Map<String, dynamic> _deriveRollingKeysWorker(Map<String, dynamic> params) {
  final String masterPrivBase64 = params['masterPrivBase64'];
  final String symKeyBase64 = params['symKeyBase64'];
  final int count = params['count'];

  final masterBytes = base64Decode(masterPrivBase64);
  // We need to access pc_utils. decodeBigIntWithSign is available via import.
  final masterInt = pc_utils.decodeBigIntWithSign(1, masterBytes);
  Uint8List sym = Uint8List.fromList(base64Decode(symKeyBase64));

  final List<Map<String, dynamic>> out = [];
  for (int i = 0; i < count; i++) {
    final derived = FindMyController.deriveNextKeys(masterInt, sym);
    // derived: {nextSymKey, advKey, privateKey, publicKey}
    final ECPublicKey publicKey = derived['publicKey'];
    final ECPrivateKey privateKey = derived['privateKey'];

    // We can compute hash here or in main thread. Computing here saves main thread time.
    final hashedPublicKey =
        FindMyController.getHashedPublicKey(publicKey: publicKey);

    // We send back raw data needed to reconstruct FindMyKeyPair
    // FindMyKeyPair needs: publicKey (derived from priv), hashedPublicKey, privateKey, date, battery
    // We just send privateKey d (BigInt) and hash.
    out.add({
      'privateKeyInt': privateKey.d.toString(),
      'hashedPublicKey': hashedPublicKey,
      // Pass the encoded public key back to avoid re-computation
      'publicKeyBytes': publicKey.Q!.getEncoded(false),
    });

    sym = derived['nextSymKey'];
  }

  return {'keys': out, 'nextSymmetricKey': base64Encode(sym)};
}

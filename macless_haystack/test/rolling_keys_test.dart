import 'package:flutter_test/flutter_test.dart';
import 'package:macless_haystack/findMy/find_my_controller.dart';
import 'package:macless_haystack/findMy/models.dart';
import 'package:pointycastle/export.dart';
import 'package:flutter/services.dart';
import 'dart:convert';
import 'dart:typed_data';

// Helper to print hex
String toHex(Uint8List bytes) {
  return bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join('');
}

void main() {
  test('verify rolling key sequence', () async {
    // Data from user request
    const privateKeyBase64 = 'Fbue2BEapEb42SY51d1RCf/yvQ1Q1xNSK9vAFA==';
    const symmetricKeyBase64 = 'k0RUFuwHilvqWgSma861FAGUsOlMiJko+Ks/T+lViLo=';

    final masterBytes = base64Decode(privateKeyBase64);
    var masterInt = BigInt.zero;
    for (var i = 0; i < masterBytes.length; i++) {
      masterInt = (masterInt << 8) | BigInt.from(masterBytes[i]);
    }

    Uint8List symKey = base64Decode(symmetricKeyBase64);
    print('Start Sym Key: ${toHex(symKey)}');

    // Expected sequence from log:
    // 1. Next Sym: 33827560bd84b9587d0c16e7e6d5cd84fabb20424274996dce100736b17c9e81
    //    Adv Key: efdda41efe08e9a162a7deacbad194f331a326b137a82bd8cbaa1b9d
    // 2. Next Sym: cd4b03f8557a9a82c606f911931fcded4a4d01e57525edcd7c46c36174ca8836
    //    Adv Key: 5b72760feab88f2a067f47ec510505295706532ce5df5adc7ed92620
    // 3. Next Sym: 9061eb5e57bd40b62e94c1d965b719522599c47ba21d47adfc8c1c0522e843b2
    //    Adv Key: b130e94d96b8e5ed869d875b38c040fb1abdf9674255ec2f02ea1d04
    // 4. Next Sym: 39f1963df38e75f8d70ceb268140eb1a713f77c5f3d95d2bffb9f33c44f2ebb6
    //    Adv Key: 6c5b5c1c894282206c1a97744df0c4b5fa800f65044aec98ca22c8a6

    final expectedSym = [
      '33827560bd84b9587d0c16e7e6d5cd84fabb20424274996dce100736b17c9e81',
      'cd4b03f8557a9a82c606f911931fcded4a4d01e57525edcd7c46c36174ca8836',
      '9061eb5e57bd40b62e94c1d965b719522599c47ba21d47adfc8c1c0522e843b2',
      '39f1963df38e75f8d70ceb268140eb1a713f77c5f3d95d2bffb9f33c44f2ebb6'
    ];
    final expectedAdv = [
      'efdda41efe08e9a162a7deacbad194f331a326b137a82bd8cbaa1b9d',
      '5b72760feab88f2a067f47ec510505295706532ce5df5adc7ed92620',
      'b130e94d96b8e5ed869d875b38c040fb1abdf9674255ec2f02ea1d04',
      '6c5b5c1c894282206c1a97744df0c4b5fa800f65044aec98ca22c8a6'
    ];

    for (int i = 0; i < 4; i++) {
      final result = FindMyController.deriveNextKeys(masterInt, symKey);

      final nextSymKey = result['nextSymKey'] as Uint8List;
      final advKey = result['advKey'] as Uint8List;

      String nextSymHex = toHex(nextSymKey);
      String advHex = toHex(advKey);

      print('Round ${i + 1}');
      print('  NextSym: $nextSymHex');
      print('  AdvKey : $advHex');

      expect(nextSymHex, expectedSym[i],
          reason: 'Symmetric Key Mismatch at round ${i + 1}');
      expect(advHex, expectedAdv[i],
          reason: 'Adv Key Mismatch at round ${i + 1}');

      symKey = nextSymKey;
    }

    // Test deriveRollingKeys (bulk) method as well
    TestWidgetsFlutterBinding.ensureInitialized();
    const channel =
        MethodChannel('plugins.it_nomads.com/flutter_secure_storage');
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall methodCall) async {
      return null;
    });

    final bulkResult = await FindMyController.deriveRollingKeys(
        privateKeyBase64, symmetricKeyBase64, 4);
    final keys = bulkResult['keys'] as List<FindMyKeyPair>;

    for (int i = 0; i < 4; i++) {

      Uint8List advBytes = hexToBytes(expectedAdv[i]);
      final digest = SHA256Digest();
      final expectedHash = digest.process(advBytes);
      final expectedHashBase64 = base64Encode(expectedHash);

      print(
          'Round ${i + 1}: Expected HashID: $expectedHashBase64 vs Generated: ${keys[i].hashedPublicKey}');

      expect(keys[i].hashedPublicKey, expectedHashBase64,
          reason:
              'Hashed Public Key does not match expected SHA256(AdvKey) for round ${i + 1}');
    }
  });
}

Uint8List hexToBytes(String hex) {
  var result = Uint8List(hex.length ~/ 2);
  for (var i = 0; i < hex.length; i += 2) {
    var num = int.parse(hex.substring(i, i + 2), radix: 16);
    result[i ~/ 2] = num;
  }
  return result;
}

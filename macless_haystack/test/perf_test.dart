import 'package:flutter_test/flutter_test.dart';
import 'package:macless_haystack/findMy/find_my_controller.dart';
import 'package:flutter/services.dart';

void main() {
  test('performance of deriveRollingKeys', () async {
    TestWidgetsFlutterBinding.ensureInitialized();
    // data from existing test
    const privateKeyBase64 = 'Fbue2BEapEb42SY51d1RCf/yvQ1Q1xNSK9vAFA==';
    const symmetricKeyBase64 = 'k0RUFuwHilvqWgSma861FAGUsOlMiJko+Ks/T+lViLo=';

    final stopwatch = Stopwatch()..start();
    final count = 1000;

    // Mock channel for secure storage if needed (though deriveRollingKeys doesn't use it directly)
    const channel =
        MethodChannel('plugins.it_nomads.com/flutter_secure_storage');
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall methodCall) async {
      return null;
    });

    final result = await FindMyController.deriveRollingKeys(
        privateKeyBase64, symmetricKeyBase64, count);
    stopwatch.stop();

    print('Generated $count keys in ${stopwatch.elapsedMilliseconds}ms');

    // Verify we got the public key bytes populated
    final keys = result['keys'] as List;
    expect(keys.length, count);

    // Check if it was reasonably fast.
    // Without the fix, 1000 keys would do 1000 point multiplications on main thread.
    // With the fix, it does 0 point multiplications on main thread (just decodes).
    // Point multiplication in Dart is slow (e.g. 5-10ms per op?). 1000 * 5ms = 5000ms.
    // With fix, it should be dominated by isolate serialization cost + decode.
    expect(stopwatch.elapsedMilliseconds, lessThan(4000));
  });
}

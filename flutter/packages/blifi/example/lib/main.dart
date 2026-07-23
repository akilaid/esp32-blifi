// Minimal placeholder app. The full provisioning UI is built in Phase 6; this
// exists so the package can run on a device (and host the interop test) and to
// satisfy pub.dev's example requirement.
import 'package:flutter/material.dart';

void main() => runApp(const BlifiExampleApp());

class BlifiExampleApp extends StatelessWidget {
  const BlifiExampleApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'blifi example',
      theme: ThemeData(colorSchemeSeed: Colors.blue, useMaterial3: true),
      home: Scaffold(
        appBar: AppBar(title: const Text('blifi example')),
        body: const Center(
          child: Padding(
            padding: EdgeInsets.all(24),
            child: Text(
              'blifi provisioning example.\n\n'
              'The full UI ships in Phase 6. See integration_test/ for the '
              'device interop test against the firmware.',
              textAlign: TextAlign.center,
            ),
          ),
        ),
      ),
    );
  }
}

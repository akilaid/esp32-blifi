import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';

/// A framed QR viewfinder with an animated scan line.
class ViewfinderOverlay extends StatelessWidget {
  const ViewfinderOverlay({super.key});

  @override
  Widget build(BuildContext context) {
    final primary = Theme.of(context).colorScheme.primary;
    const size = 260.0;
    return Center(
      child: SizedBox(
        width: size,
        height: size,
        child: Stack(
          children: [
            Container(
              decoration: BoxDecoration(
                border: Border.all(color: Colors.white.withValues(alpha: 0.85), width: 2),
                borderRadius: BorderRadius.circular(28),
              ),
            ),
            Align(
              alignment: Alignment.topCenter,
              child: Container(
                height: 3,
                width: size - 24,
                margin: const EdgeInsets.only(top: 10),
                decoration: BoxDecoration(
                  color: primary,
                  borderRadius: BorderRadius.circular(2),
                  boxShadow: [BoxShadow(color: primary.withValues(alpha: 0.6), blurRadius: 8)],
                ),
              )
                  .animate(onPlay: (c) => c.repeat(reverse: true))
                  .moveY(begin: 0, end: size - 30, duration: 1500.ms, curve: Curves.easeInOut),
            ),
          ],
        ),
      ),
    );
  }
}

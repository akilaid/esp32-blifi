import 'package:flutter/material.dart';
import 'package:flutter_animate/flutter_animate.dart';

/// A vertical step timeline: past steps checked, the current one active
/// (spinner), future steps pending - so progress always reads as moving.
class StepTimeline extends StatelessWidget {
  const StepTimeline({super.key, required this.steps, required this.activeIndex});

  final List<String> steps;
  final int activeIndex;

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        for (int i = 0; i < steps.length; i++) ...[
          _stepRow(context, i, scheme),
          if (i < steps.length - 1)
            Padding(
              padding: const EdgeInsets.only(left: 13),
              child: Container(
                width: 2,
                height: 22,
                color: i < activeIndex ? scheme.primary : scheme.outlineVariant,
              ),
            ),
        ],
      ],
    );
  }

  Widget _stepRow(BuildContext context, int i, ColorScheme scheme) {
    final done = i < activeIndex;
    final active = i == activeIndex;

    Widget dot;
    if (done) {
      dot = Container(
        width: 28,
        height: 28,
        decoration: BoxDecoration(color: scheme.primary, shape: BoxShape.circle),
        child: Icon(Icons.check_rounded, size: 18, color: scheme.onPrimary),
      ).animate(key: ValueKey('done_$i')).scale(
            begin: const Offset(0.5, 0.5),
            end: const Offset(1, 1),
            duration: 320.ms,
            curve: Curves.easeOutBack,
          );
    } else if (active) {
      dot = const SizedBox(width: 24, height: 24, child: CircularProgressIndicator(strokeWidth: 3));
    } else {
      dot = Container(
        width: 16,
        height: 16,
        decoration: BoxDecoration(color: scheme.surfaceContainerHighest, shape: BoxShape.circle),
      );
    }

    return Row(
      children: [
        SizedBox(width: 28, height: 28, child: Center(child: dot)),
        const SizedBox(width: 16),
        Text(
          steps[i],
          style: TextStyle(
            fontWeight: active ? FontWeight.w600 : FontWeight.w400,
            color: (done || active) ? scheme.onSurface : scheme.onSurfaceVariant,
          ),
        ),
      ],
    );
  }
}

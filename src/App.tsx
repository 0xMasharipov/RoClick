import React, {useEffect, useMemo, useRef, useState} from 'react';
import {
  Image, PanResponder, Pressable, ScrollView, StyleSheet, Switch, Text,
  TextInput, View, type StyleProp, type ViewStyle,
} from 'react-native';
import type {} from 'react-native-windows';
import Engine, {type ClickConfig, type EngineState, type Theme} from './native/NativeClickEngine';

const defaults: ClickConfig = {intervalMs: 10, pressMs: 0, button: 'left', lockToTarget: true};
type Colors = {bg: string; panel: string; text: string; secondary: string; line: string; blue: string; onBlue: string; danger: string};
const light: Colors = {bg: '#F0F2F6', panel: '#FFFFFF', text: '#182130', secondary: '#586375', line: '#CED4DE', blue: '#0067DC', onBlue: '#FFFFFF', danger: '#B42332'};
const dark: Colors = {bg: '#171A20', panel: '#252A33', text: '#F3F5FA', secondary: '#ADB7C8', line: '#626D7E', blue: '#82B7FF', onBlue: '#102440', danger: '#FF9DA7'};

// Keep numeric labels consistent on Windows Hermes without locale support.
const wholeNumber = (value: number) => Math.trunc(value).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
const configuredRate = (interval: number) => {
  const [whole, decimal] = (1000 / interval).toFixed(1).split('.');
  return wholeNumber(Number(whole)) + (decimal === '0' ? '' : `.${decimal}`);
};

function Action({label, onPress, colors, primary, disabled, style, selected, expanded}: {
  label: string; onPress: () => void; colors: Colors; primary?: boolean; disabled?: boolean;
  style?: StyleProp<ViewStyle>; selected?: boolean; expanded?: boolean;
}) {
  const [focused, setFocused] = useState(false);
  return <Pressable accessibilityRole="button" accessibilityLabel={label}
    accessibilityState={{disabled, selected, expanded}} disabled={disabled}
    onAccessibilityTap={disabled ? undefined : onPress}
    onFocus={() => setFocused(true)} onBlur={() => setFocused(false)} onPress={onPress}
    style={({pressed}) => [styles.action, {backgroundColor: primary ? colors.blue : colors.panel,
      borderColor: focused ? colors.text : selected ? colors.blue : colors.line,
      borderWidth: focused || selected ? 2 : 1, opacity: disabled ? 0.55 : pressed ? 0.8 : 1}, style]}>
    <Text style={[styles.actionText, {color: primary ? colors.onBlue : colors.text}]}>{label}</Text>
  </Pressable>;
}

function SpeedSlider({interval, onChange, colors}: {interval: number; onChange: (ms: number) => void; colors: Colors}) {
  const [width, setWidth] = useState(1);
  const dragStart = useRef(0);
  const fraction = 1 - Math.log10(interval) / 3;
  const fromPosition = (x: number) => onChange(Math.max(1, Math.min(1000, Math.round(10 ** (3 * (1 - Math.max(0, Math.min(1, x / width))))))));
  const changeRef = useRef(fromPosition);
  changeRef.current = fromPosition;
  const responder = useMemo(() => PanResponder.create({
    onStartShouldSetPanResponder: () => true,
    onMoveShouldSetPanResponder: () => true,
    onPanResponderGrant: event => { dragStart.current = event.nativeEvent.locationX; changeRef.current(dragStart.current); },
    onPanResponderMove: (_event, gesture) => changeRef.current(dragStart.current + gesture.dx),
  }), []);
  return <View {...responder.panHandlers} accessible focusable accessibilityRole="adjustable"
    accessibilityLabel="Click speed" accessibilityHint="Use arrow keys to change the interval by one millisecond. Home is slowest; End is fastest."
    accessibilityValue={{min: 1, max: 1000, now: 1000 / interval, text: `${(1000 / interval).toFixed(1)} clicks per second, ${interval} milliseconds`}}
    accessibilityActions={[{name: 'increment'}, {name: 'decrement'}]}
    onAccessibilityAction={e => onChange(Math.max(1, Math.min(1000, interval + (e.nativeEvent.actionName === 'increment' ? -1 : 1))))}
    onKeyDown={e => {
      const key = e.nativeEvent.key;
      if (['ArrowRight', 'ArrowUp', 'ArrowLeft', 'ArrowDown', 'Home', 'End'].includes(key)) {
        onChange(key === 'Home' ? 1000 : key === 'End' ? 1 : Math.max(1, Math.min(1000, interval + (key === 'ArrowRight' || key === 'ArrowUp' ? -1 : 1))));
      }
    }}
    keyDownEvents={['ArrowRight', 'ArrowUp', 'ArrowLeft', 'ArrowDown', 'Home', 'End'].map(code => ({code}))}
    enableFocusRing
    onLayout={e => setWidth(Math.max(1, e.nativeEvent.layout.width - 28))}
    style={[styles.slider, {borderColor: 'transparent'}]}>
    <View pointerEvents="none" style={[styles.track, {backgroundColor: colors.line}]}>
      <View style={[styles.fill, {width: `${fraction * 100}%`, backgroundColor: colors.blue}]} />
    </View>
    <View pointerEvents="none" style={[styles.thumb, {left: 2 + fraction * width, borderColor: colors.blue, backgroundColor: colors.panel}]} />
  </View>;
}

export default function App() {
  const [config, setConfig] = useState(defaults);
  const configRef = useRef(config);
  const [state, setState] = useState<EngineState>();
  const [intervalText, setIntervalText] = useState('10');
  const [pressText, setPressText] = useState('0');
  const [validation, setValidation] = useState('');
  const [advanced, setAdvanced] = useState(false);
  const [theme, setTheme] = useState<Theme>('system');
  useEffect(() => {
    const subscription = Engine.onState(next => { setState(next); Engine.acknowledge(); });
    Engine.observe(true);
    return () => { Engine.observe(false); Engine.stop(); subscription.remove(); };
  }, []);
  const colors: Colors = state?.highContrast ? {
    bg: state.windowColor, panel: state.buttonColor, text: state.buttonTextColor,
    secondary: state.buttonTextColor, line: state.buttonTextColor,
    blue: state.highlightColor, onBlue: state.highlightTextColor, danger: state.buttonTextColor,
  } : (state?.dark ?? theme === 'dark') ? dark : light;
  const outsideText = state?.highContrast ? state.textColor : colors.text;
  const outsideSecondary = state?.highContrast ? state.textColor : colors.secondary;
  const active = state?.state === 'running' || state?.state === 'paused';
  const capturing = state?.state === 'capturing';
  const update = (patch: Partial<ClickConfig>) => {
    const next = {...configRef.current, ...patch};
    if (next.pressMs >= next.intervalMs) {
      setValidation('Set press duration below the new interval first.'); return;
    }
    configRef.current = next; setConfig(next); setValidation('');
    setIntervalText(String(next.intervalMs)); setPressText(String(next.pressMs));
    Engine.configure(next);
  };
  const commit = (field: 'intervalMs' | 'pressMs', value: string) => {
    const n = Number(value);
    if (!/^\d+$/.test(value) || !Number.isInteger(n) || n < (field === 'intervalMs' ? 1 : 0) || n > (field === 'intervalMs' ? 1000 : config.intervalMs - 1)) {
      setValidation(field === 'intervalMs' ? 'Enter a whole interval from 1 to 1,000 ms.' : `Enter a whole press duration from 0 to ${config.intervalMs - 1} ms.`);
      return;
    }
    update({[field]: n});
  };
  const caption = state?.state === 'running' ? 'Clicking' : state?.state === 'paused' ? 'Paused · target not focused' : capturing ? 'Focus your target now…' : state?.state === 'error' ? 'Stopped · needs attention' : state ? 'Ready when you are' : 'Connecting to click engine…';
  const primaryLabel = active ? 'Stop clicking' : capturing ? 'Cancel capture' : state?.held ? 'Retry button release' : 'Start clicking';
  const canStart = !!state && !validation && (!config.lockToTarget || state.targetPid !== 0);
  return <ScrollView style={{flex: 1, backgroundColor: state?.acrylic && !state.highContrast ? 'transparent' : colors.bg}}
    contentContainerStyle={styles.content} keyboardShouldPersistTaps="handled">
    <View style={styles.header}>
      <Image source={require('../assets/roclick-logo.png')} style={styles.logo} accessible={false} />
      <Text style={[styles.brand, {color: outsideText}]}>RoClick</Text>
    </View>

    <View style={[styles.panel, {backgroundColor: colors.panel, borderColor: colors.line}]}>
      <Text accessibilityLiveRegion="polite" style={[styles.status, {color: colors.text}]}>{caption}</Text>
      <Text style={[styles.hint, {color: colors.secondary}]}>1 starts · 2 stops · F6 toggles · Esc stops</Text>
      <Action label={primaryLabel} primary colors={colors}
        disabled={!active && !capturing && !state?.held && !canStart}
        onPress={() => active || capturing || state?.held ? Engine.stop() : Engine.start()} style={styles.start} />
      <View style={styles.rateRow}>
        <View style={styles.grow}>
          <Text style={[styles.rate, {color: colors.text}]}>{state ? state.submittedRate.toFixed(1) : '—'}<Text style={styles.rateUnit}> / sec</Text></Text>
          <Text style={[styles.small, {color: colors.secondary}]}>Submitted to Windows</Text>
        </View>
        <View style={styles.total}>
          <Text style={[styles.totalNumber, {color: colors.text}]}>{state ? wholeNumber(state.submittedClicks) : '—'}</Text>
          <Text style={[styles.small, {color: colors.secondary}]}>This session</Text>
        </View>
      </View>
      <Text style={[styles.small, {color: colors.secondary}]}>Applications may receive or process fewer clicks.</Text>
    </View>

    {!!state?.error && <View accessibilityRole="alert" style={[styles.error, {backgroundColor: colors.panel, borderColor: colors.danger}]}>
      <Text selectable style={{color: colors.danger}}>{state.error}{state.errorCode ? ` (Windows code ${state.errorCode})` : ''}</Text>
    </View>}

    <View style={[styles.panel, {backgroundColor: colors.panel, borderColor: colors.line}]}>
      <View style={styles.row}>
        <View style={styles.grow}><Text style={[styles.label, {color: colors.text}]}>Click speed</Text>
          <Text style={[styles.hint, {color: colors.secondary}]}>{configuredRate(config.intervalMs)} clicks/sec configured</Text></View>
        <TextInput accessibilityLabel="Click interval in milliseconds" value={intervalText}
          selectTextOnFocus keyboardType="number-pad" maxLength={4}
          onChangeText={text => { if (text !== intervalText) { setIntervalText(text); setValidation('Apply a valid interval with Enter or Tab.'); } }}
          onEndEditing={() => commit('intervalMs', intervalText)} onSubmitEditing={() => commit('intervalMs', intervalText)}
          style={[styles.input, {color: colors.text, borderColor: colors.line, backgroundColor: colors.bg}]} />
        <Text style={{color: colors.secondary}}>ms</Text>
      </View>
      <SpeedSlider interval={config.intervalMs} onChange={intervalMs => update({intervalMs})} colors={colors} />
      <View style={[styles.row, styles.sliderLabels]}><Text style={[styles.small, {color: colors.secondary}]}>1 / sec</Text><Text style={[styles.small, {color: colors.secondary}]}>1,000 / sec</Text></View>
      {!!validation && <Text accessibilityLiveRegion="polite" style={[styles.validation, {color: colors.danger}]}>{validation}</Text>}
      <Text style={[styles.label, styles.sectionGap, {color: colors.text}]}>Mouse button</Text>
      <View style={styles.segments}>
        {(['left', 'right', 'middle'] as const).map(button => <Action key={button}
          label={button[0].toUpperCase() + button.slice(1)} selected={config.button === button}
          colors={colors} onPress={() => update({button})} style={styles.grow} />)}
      </View>
      <View style={[styles.target, {borderColor: colors.line}]}>
        <Text style={[styles.label, {color: colors.text}]}>Target window</Text>
        <View style={[styles.row, styles.captureRow]}><View style={styles.grow}>
          <Text selectable numberOfLines={2} style={[styles.targetTitle, {color: colors.text}]}>{capturing ? 'Switch windows now…' : state?.targetTitle || 'No window captured'}</Text>
          <Text style={[styles.small, {color: colors.secondary}]}>{state?.targetPid ? `Process ${state.targetPid}` : 'Capture in 2 seconds'}</Text>
        </View>
          <Action label={capturing ? 'Cancel capture' : state?.targetPid ? 'Recapture window' : 'Capture window'} colors={colors}
            onPress={() => capturing ? Engine.stop() : Engine.captureTarget(2000)} />
        </View>
        {!!state?.targetPid && <Text style={[styles.hint, {color: colors.secondary}]}>{config.lockToTarget ? 'Pauses when the target loses focus.' : 'Focus lock is off.'}</Text>}
      </View>
      <Action label={advanced ? 'Advanced settings −' : 'Advanced settings +'} expanded={advanced} colors={colors}
        onPress={() => setAdvanced(!advanced)} style={styles.advancedButton} />
      {advanced && <View style={styles.advanced}>
        <View style={styles.row}>
          <Text style={[styles.label, styles.grow, {color: colors.text}]}>Press duration</Text>
          <TextInput accessibilityLabel="Press duration in milliseconds" value={pressText} keyboardType="number-pad" maxLength={3} selectTextOnFocus
            onChangeText={text => { if (text !== pressText) { setPressText(text); setValidation('Apply a valid press duration with Enter or Tab.'); } }}
            onEndEditing={() => commit('pressMs', pressText)} onSubmitEditing={() => commit('pressMs', pressText)}
            style={[styles.input, {color: colors.text, backgroundColor: colors.bg, borderColor: colors.line}]} />
          <Text style={{color: colors.secondary}}>ms</Text>
        </View>
        <Text style={[styles.hint, {color: colors.secondary}]}>0 ms submits down and up together for maximum speed. Longer presses reduce available speed and must stay below the interval.</Text>
        <View style={[styles.row, styles.sectionGap]}>
          <Text style={[styles.grow, {color: colors.text}]}>Only click while target is focused</Text>
          <Switch accessibilityLabel="Only click while target is focused" value={config.lockToTarget} onValueChange={lockToTarget => update({lockToTarget})} />
        </View>
        {!!state?.targetPid && <Action label="Clear captured window" colors={colors} onPress={() => Engine.clearTarget()} style={styles.sectionGap} />}
        <Text style={[styles.label, styles.sectionGap, {color: colors.text}]}>Appearance</Text>
        <View style={styles.segments}>{(['system', 'light', 'dark'] as const).map(t => <Action key={t} colors={colors}
          label={t[0].toUpperCase() + t.slice(1)} selected={t === theme} style={styles.grow}
          onPress={() => { setTheme(t); Engine.setTheme(t); }} />)}</View>
        <Text style={[styles.hint, {color: colors.secondary}]}>{state?.highResolution ? 'High-resolution timer' : 'Standard timer fallback'} · {state?.acrylic ? 'Native Acrylic' : 'Solid backdrop'}</Text>
        <Text style={[styles.hint, {color: colors.secondary}]}>{state?.skipped ?? 0} deadlines skipped · {state?.failures ?? 0} errors</Text>
        <Text style={[styles.hint, {color: colors.secondary}]}>1 starts only outside RoClick. 2, Escape and F6 work everywhere. Typing 2 also stops clicking.</Text>
        <Text style={[styles.hint, {color: colors.secondary}]}>1,000 clicks/sec is a performance target, not a real-time guarantee.</Text>
      </View>}
    </View>
    <Text style={[styles.footer, {color: outsideSecondary}]}>Local only · No telemetry · No automatic updates</Text>
  </ScrollView>;
}

const styles = StyleSheet.create({
  content: {padding: 20, gap: 14, width: '100%', maxWidth: 640, alignSelf: 'center'},
  header: {flexDirection: 'row', alignItems: 'center', gap: 10, flexWrap: 'wrap', paddingBottom: 2},
  logo: {width: 30, height: 30, resizeMode: 'contain'}, brand: {fontSize: 20, fontWeight: '700'},
  panel: {borderRadius: 22, borderWidth: 1, padding: 18},
  status: {fontSize: 19, fontWeight: '600'}, hint: {fontSize: 12, lineHeight: 18, marginTop: 5},
  label: {fontSize: 14, fontWeight: '600'}, small: {fontSize: 12, lineHeight: 18},
  action: {minHeight: 40, paddingHorizontal: 14, paddingVertical: 9, borderRadius: 12, justifyContent: 'center', alignItems: 'center'},
  actionText: {fontSize: 14, fontWeight: '600', textAlign: 'center'},
  start: {minHeight: 52, marginTop: 18, borderRadius: 15},
  row: {flexDirection: 'row', alignItems: 'center', gap: 9}, grow: {flex: 1, minWidth: 0},
  rateRow: {flexDirection: 'row', alignItems: 'center', marginTop: 20, marginBottom: 14},
  rate: {fontSize: 32, fontWeight: '600', fontVariant: ['tabular-nums']}, rateUnit: {fontSize: 15, fontWeight: '400'},
  total: {alignItems: 'flex-end'}, totalNumber: {fontSize: 18, fontVariant: ['tabular-nums']},
  input: {width: 68, height: 42, borderRadius: 10, borderWidth: 1, padding: 8, textAlign: 'center', fontSize: 16},
  slider: {height: 42, marginTop: 8, borderWidth: 2, borderRadius: 8, justifyContent: 'center', paddingHorizontal: 12},
  track: {height: 5, borderRadius: 3}, fill: {height: 5, borderRadius: 3},
  thumb: {position: 'absolute', top: 6, width: 26, height: 26, borderRadius: 13, borderWidth: 3},
  sliderLabels: {justifyContent: 'space-between'}, sectionGap: {marginTop: 20},
  segments: {flexDirection: 'row', gap: 6, marginTop: 10},
  target: {borderTopWidth: 1, paddingTop: 16, marginTop: 18}, targetTitle: {fontSize: 14, marginBottom: 3},
  captureRow: {marginTop: 10}, advancedButton: {marginTop: 16}, advanced: {marginTop: 18},
  validation: {fontSize: 12, marginTop: 10}, error: {borderWidth: 1, padding: 16, borderRadius: 14},
  footer: {fontSize: 11, textAlign: 'center'},
});

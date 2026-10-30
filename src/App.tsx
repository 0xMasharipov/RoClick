import React, {useEffect, useMemo, useRef, useState} from 'react';
import {Image, PanResponder, Pressable, ScrollView, StyleSheet, Switch, Text,
  TextInput, View, type StyleProp, type ViewStyle} from 'react-native';
import type {} from 'react-native-windows';
import Engine, {type ClickConfig, type EngineState, type Theme} from './native/NativeClickEngine';
import AppSettings, {type Locale} from './native/NativeAppSettings';
import {translate, formatNumber, formatRate, type MessageKey} from './i18n';

const defaults: ClickConfig = {intervalMs: 10, pressMs: 0, button: 'left', lockToTarget: true};
type Colors = {bg: string; panel: string; text: string; secondary: string; line: string; blue: string; onBlue: string; danger: string};
const light: Colors = {bg: '#EEF2F7', panel: '#FFFFFF', text: '#142337', secondary: '#506079', line: '#C6D1DF', blue: '#006C85', onBlue: '#FFFFFF', danger: '#B42332'};
const dark: Colors = {bg: '#101722', panel: '#1B2533', text: '#F1F6FC', secondary: '#A9B9CE', line: '#4A5B72', blue: '#6AD9ED', onBlue: '#10212C', danger: '#FF9CA8'};

function Action({label, onPress, colors, primary, disabled, style, selected, expanded, id, compact}: {
  label: string; onPress: () => void; colors: Colors; primary?: boolean; disabled?: boolean;
  style?: StyleProp<ViewStyle>; selected?: boolean; expanded?: boolean; id: string; compact?: boolean;
}) {
  const [focused, setFocused] = useState(false);
  return <Pressable testID={id} accessibilityRole="button" accessibilityLabel={label}
    accessibilityState={{disabled, selected, expanded}} disabled={disabled}
    onAccessibilityTap={disabled ? undefined : onPress}
    onFocus={() => setFocused(true)} onBlur={() => setFocused(false)} onPress={onPress}
    style={({pressed}) => [styles.action, compact && styles.compact, {
      backgroundColor: primary ? colors.blue : colors.panel,
      borderColor: focused ? colors.text : selected ? colors.blue : colors.line,
      borderWidth: focused || selected ? 2 : 1, opacity: disabled ? 0.55 : pressed ? 0.8 : 1}, style]}>
    <Text style={[styles.actionText, compact && {fontSize: 12}, {color: primary ? colors.onBlue : colors.text}]}>{label}</Text>
  </Pressable>;
}
function SpeedSlider({interval, onChange, colors, locale}: {interval: number; onChange: (ms: number) => void; colors: Colors; locale: Locale}) {
  const t = (key: MessageKey, values?: Record<string, string | number>) => translate(locale, key, values);
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
    testID="speed-slider" accessibilityLabel={t('speed')} accessibilityHint={t('sliderHint')}
    accessibilityValue={{min: 1, max: 1000, now: 1000 / interval, text: t('sliderValue', {rate: formatNumber(1000 / interval, locale, 1), interval})}}
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
  const [locale, setLocale] = useState<Locale>();
  const [saveFailed, setSaveFailed] = useState(false);
  const saves = useRef(Promise.resolve());
  const saveVersion = useRef(0);
  useEffect(() => {
    let mounted = true;
    AppSettings.getLanguage().then(value => { if (mounted) setLocale(value === 'tr' ? 'tr' : 'en'); })
      .catch(() => { if (mounted) { setLocale('en'); setSaveFailed(true); } });
    return () => { mounted = false; };
  }, []);
  const changeLanguage = (value: Locale) => {
    setLocale(value); setSaveFailed(false);
    const version = ++saveVersion.current;
    saves.current = saves.current.catch(() => {}).then(() => AppSettings.setLanguage(value))
      .catch(() => { if (saveVersion.current === version) setSaveFailed(true); });
  };
  if (!locale) return <View style={styles.loading}><Text style={styles.loadingText}>RoClick</Text></View>;
  return <Controls locale={locale} onLanguage={changeLanguage} saveFailed={saveFailed} />;
}

function Controls({locale, onLanguage, saveFailed}: {locale: Locale; onLanguage: (value: Locale) => void; saveFailed: boolean}) {
  const t = (key: MessageKey, values?: Record<string, string | number>) => translate(locale, key, values);
  const [config, setConfig] = useState(defaults);
  const configRef = useRef(config);
  const [state, setState] = useState<EngineState>();
  const [intervalText, setIntervalText] = useState('10');
  const [pressText, setPressText] = useState('0');
  const [validation, setValidation] = useState<'' | 'durationBelow' | 'applyInterval' | 'applyPress' | 'invalidInterval' | 'invalidPress'>('');
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
  const active = state?.state === 'running' || state?.state === 'paused';
  const capturing = state?.state === 'capturing';
  const panel = [styles.panel, {backgroundColor: colors.panel, borderColor: colors.line}];
  const update = (patch: Partial<ClickConfig>) => {
    const next = {...configRef.current, ...patch};
    if (next.pressMs >= next.intervalMs) { setValidation('durationBelow'); return; }
    configRef.current = next; setConfig(next); setValidation('');
    setIntervalText(String(next.intervalMs)); setPressText(String(next.pressMs));
    Engine.configure(next);
  };
  const commit = (field: 'intervalMs' | 'pressMs', value: string) => {
    const n = Number(value);
    if (!/^\d+$/.test(value) || !Number.isInteger(n) || n < (field === 'intervalMs' ? 1 : 0) || n > (field === 'intervalMs' ? 1000 : config.intervalMs - 1)) {
      setValidation(field === 'intervalMs' ? 'invalidInterval' : 'invalidPress'); return;
    }
    update({[field]: n});
  };
  const caption: MessageKey = state?.state === 'running' ? 'running' : state?.state === 'paused' ? 'paused' : capturing ? 'capturing' : state?.state === 'error' ? 'needsAttention' : state ? 'ready' : 'connecting';
  const primary: MessageKey = active ? 'stop' : capturing ? 'cancelCapture' : state?.held ? 'retryRelease' : 'start';
  const canStart = !!state && !validation && (!config.lockToTarget || state.targetPid !== 0);
  const errorKey: MessageKey = state?.errorKey ? `error_${state.errorKey}` : 'error_unknown';
  return <ScrollView style={{flex: 1, backgroundColor: state?.acrylic && !state.highContrast ? 'transparent' : colors.bg}}
    contentContainerStyle={styles.content} keyboardShouldPersistTaps="handled">
    <View style={styles.header}>
      <View style={styles.brandRow}>
        <Image source={require('../assets/roclick-logo.png')} style={styles.logo} accessible={false} />
        <Text testID="app-header" style={[styles.brand, {color: outsideText}]}>RoClick</Text>
      </View>
      <View accessibilityLabel={t('language')} style={styles.languages}>
        <Action id="language-en" compact label="English" colors={colors} selected={locale === 'en'} onPress={() => onLanguage('en')} />
        <Action id="language-tr" compact label="Türkçe" colors={colors} selected={locale === 'tr'} onPress={() => onLanguage('tr')} />
      </View>
    </View>
    <View style={[styles.hero, {backgroundColor: state?.highContrast ? colors.panel : '#101722'}]}>
      {!state?.highContrast && <Image source={require('../assets/gaming-header.png')} style={styles.heroArt} accessible={false} />}
      <View style={styles.heroCopy}>
        <Text style={[styles.heroTitle, {color: state?.highContrast ? colors.text : '#FFFFFF'}]}>{t('tagline')}</Text>
        <Text style={[styles.heroHint, {color: state?.highContrast ? colors.text : '#C2D0E1'}]}>{t('heroHint')}</Text>
      </View>
    </View>
    {saveFailed && <Text accessibilityLiveRegion="polite" style={{color: outsideText}}>{t('languageSave')}</Text>}
    <View style={panel}>
      <View style={styles.statusRow}>
        <View style={[styles.statusDot, {backgroundColor: state?.state === 'error' ? colors.danger : colors.blue}]} />
        <Text testID="engine-status" accessibilityLiveRegion="polite" style={[styles.status, {color: colors.text}]}>{t(caption)}</Text>
      </View>
      <View style={styles.rateRow}>
        <View style={styles.grow}>
          <Text style={[styles.rate, {color: colors.text}]}>{state ? formatNumber(state.submittedRate, locale, 1) : '—'}<Text style={styles.rateUnit}> {t('perSecond')}</Text></Text>
          <Text style={[styles.small, {color: colors.secondary}]}>{t('submitted')}</Text>
        </View>
        <View style={styles.total}>
          <Text style={[styles.totalNumber, {color: colors.text}]}>{state ? formatNumber(state.submittedClicks, locale) : '—'}</Text>
          <Text style={[styles.small, {color: colors.secondary}]}>{t('session')}</Text>
        </View>
      </View>
      <Action id="primary-action" label={t(primary)} primary colors={colors}
        disabled={!active && !capturing && !state?.held && !canStart}
        onPress={() => active || capturing || state?.held ? Engine.stop() : Engine.start()} style={styles.start} />
      <Text style={[styles.hint, {color: colors.secondary}]}>{t('shortcuts')}</Text>
      <Text style={[styles.small, {color: colors.secondary, marginTop: 6}]}>{t('rateCaution')}</Text>
      <View style={[styles.target, {borderColor: colors.line}]}>
        <Text style={[styles.label, {color: colors.text}]}>{t('target')}</Text>
        <Text selectable numberOfLines={2} style={[styles.targetTitle, {color: colors.text}]}>{capturing ? t('switchWindows') : state?.targetPid ? state.targetTitle || t('untitled') : t('noTarget')}</Text>
        <View style={styles.captureRow}>
          <Text style={[styles.small, styles.grow, {color: colors.secondary}]}>{state?.targetPid ? t('process', {pid: state.targetPid}) : t('captureDelay')}</Text>
          <Action id="capture" label={t(capturing ? 'cancelCapture' : state?.targetPid ? 'recapture' : 'capture')} colors={colors}
            onPress={() => capturing ? Engine.stop() : Engine.captureTarget(2000)} style={styles.captureAction} />
        </View>
        {!!state?.targetPid && <Text style={[styles.hint, {color: colors.secondary}]}>{t(config.lockToTarget ? 'focusPause' : 'focusOff')}</Text>}
      </View>
    </View>
    {!!state?.error && <View testID="engine-error" accessibilityRole="alert" style={[styles.error, {backgroundColor: colors.panel, borderColor: colors.danger}]}>
      <Text selectable style={{color: colors.danger}}>{t(errorKey)}{state.errorCode ? ` (${t('windowsCode', {code: state.errorCode})})` : ''}</Text>
    </View>}
    <View style={panel}>
      <View style={styles.row}>
        <View style={styles.grow}><Text style={[styles.label, {color: colors.text}]}>{t('speed')}</Text>
          <Text style={[styles.hint, {color: colors.secondary}]}>{t('configured', {rate: formatRate(config.intervalMs, locale)})}</Text></View>
        <TextInput testID="interval-input" accessibilityLabel={t('intervalLabel')} value={intervalText}
          selectTextOnFocus keyboardType="number-pad" maxLength={4}
          onChangeText={text => { if (text !== intervalText) { setIntervalText(text); setValidation('applyInterval'); } }}
          onEndEditing={event => commit('intervalMs', event.nativeEvent.text)} onSubmitEditing={event => commit('intervalMs', event.nativeEvent.text)}
          style={[styles.input, {color: colors.text, borderColor: colors.line, backgroundColor: colors.bg}]} />
        <Text style={{color: colors.secondary}}>ms</Text>
      </View>
      <SpeedSlider interval={config.intervalMs} onChange={intervalMs => update({intervalMs})} colors={colors} locale={locale} />
      <View style={styles.sliderLabels}><Text style={[styles.small, {color: colors.secondary}]}>1 {t('perSecond')}</Text><Text style={[styles.small, {color: colors.secondary}]}>{formatNumber(1000, locale)} {t('perSecond')}</Text></View>
      {!!validation && <Text testID="validation" accessibilityLiveRegion="polite" style={[styles.validation, {color: colors.danger}]}>{t(validation, {max: config.intervalMs - 1})}</Text>}
      <Text style={[styles.label, styles.sectionGap, {color: colors.text}]}>{t('mouseButton')}</Text>
      <View style={styles.segments}>
        {(['left', 'right', 'middle'] as const).map(button => <Action id={`button-${button}`} key={button}
          label={t(button)} selected={config.button === button} colors={colors} onPress={() => update({button})} style={styles.grow} />)}
      </View>
    </View>
    <View style={panel}>
      <Action id="advanced" label={`${t('advanced')} ${advanced ? '−' : '+'}`} expanded={advanced} colors={colors}
        onPress={() => setAdvanced(!advanced)} />
      {advanced && <View style={styles.advanced}>
        <View style={styles.row}>
          <Text style={[styles.label, styles.grow, {color: colors.text}]}>{t('press')}</Text>
          <TextInput testID="press-input" accessibilityLabel={t('pressLabel')} value={pressText} keyboardType="number-pad" maxLength={3} selectTextOnFocus
            onChangeText={text => { if (text !== pressText) { setPressText(text); setValidation('applyPress'); } }}
            onEndEditing={event => commit('pressMs', event.nativeEvent.text)} onSubmitEditing={event => commit('pressMs', event.nativeEvent.text)}
            style={[styles.input, {color: colors.text, backgroundColor: colors.bg, borderColor: colors.line}]} />
          <Text style={{color: colors.secondary}}>ms</Text>
        </View>
        <Text style={[styles.hint, {color: colors.secondary}]}>{t('pressHint')}</Text>
        <View style={[styles.row, styles.sectionGap]}>
          <Text style={[styles.grow, {color: colors.text}]}>{t('focusLock')}</Text>
          <Switch testID="focus-lock" accessibilityLabel={t('focusLock')} value={config.lockToTarget} onValueChange={lockToTarget => update({lockToTarget})} />
        </View>
        {!!state?.targetPid && <Action id="clear-target" label={t('clearTarget')} colors={colors} onPress={() => Engine.clearTarget()} style={styles.sectionGap} />}
        <Text style={[styles.label, styles.sectionGap, {color: colors.text}]}>{t('appearance')}</Text>
        <View style={styles.segments}>{(['system', 'light', 'dark'] as const).map(value => <Action id={`theme-${value}`} key={value} colors={colors}
          label={t(value)} selected={value === theme} style={styles.grow}
          onPress={() => { setTheme(value); Engine.setTheme(value); }} />)}</View>
        <Text style={[styles.hint, {color: colors.secondary}]}>{t(state?.highResolution ? 'highTimer' : 'standardTimer')} · {t(state?.acrylic ? 'acrylic' : 'solid')}</Text>
        <Text style={[styles.hint, {color: colors.secondary}]}>{t('diagnostics', {skipped: formatNumber(state?.skipped ?? 0, locale), failures: formatNumber(state?.failures ?? 0, locale)})}</Text>
        <Text style={[styles.hint, {color: colors.secondary}]}>{t('shortcutHint')}</Text>
        <Text style={[styles.hint, {color: colors.secondary}]}>{t('performance')}</Text>
      </View>}
    </View>
    <Text style={[styles.footer, {color: state?.highContrast ? state.textColor : colors.secondary}]}>{t('footer')}</Text>
  </ScrollView>;
}

const styles = StyleSheet.create({
  loading: {flex: 1, backgroundColor: '#101722', justifyContent: 'center', alignItems: 'center'},
  loadingText: {color: '#FFFFFF', fontSize: 24, fontWeight: '700'},
  content: {padding: 16, gap: 12, width: '100%', maxWidth: 640, alignSelf: 'center'},
  header: {flexDirection: 'row', alignItems: 'center', justifyContent: 'space-between', gap: 8, flexWrap: 'wrap'},
  brandRow: {flexDirection: 'row', gap: 8, alignItems: 'center'},
  languages: {flexDirection: 'row', gap: 4}, logo: {width: 28, height: 28}, brand: {fontSize: 21, fontWeight: '700'},
  hero: {height: 96, borderRadius: 16, overflow: 'hidden', justifyContent: 'center'},
  heroArt: {position: 'absolute', width: '100%', height: '100%', resizeMode: 'cover'},
  heroCopy: {width: '52%', padding: 12}, heroTitle: {fontSize: 19, lineHeight: 23, fontWeight: '700'}, heroHint: {fontSize: 11, lineHeight: 15, marginTop: 5},
  panel: {borderRadius: 16, borderWidth: 1, padding: 16},
  statusRow: {flexDirection: 'row', alignItems: 'center', gap: 8},
  statusDot: {width: 7, height: 7, borderRadius: 4}, status: {fontSize: 15, fontWeight: '600', flex: 1},
  hint: {fontSize: 12, lineHeight: 18, marginTop: 5}, label: {fontSize: 14, fontWeight: '600'},
  small: {fontSize: 11, lineHeight: 16},
  action: {minHeight: 40, paddingHorizontal: 12, paddingVertical: 9, borderRadius: 10, justifyContent: 'center', alignItems: 'center'},
  compact: {paddingHorizontal: 9, paddingVertical: 6, minHeight: 36},
  actionText: {fontSize: 14, fontWeight: '600', textAlign: 'center'},
  start: {minHeight: 46, marginTop: 12}, row: {flexDirection: 'row', alignItems: 'center', gap: 8}, grow: {flex: 1, minWidth: 0},
  rateRow: {flexDirection: 'row', alignItems: 'center', marginTop: 12},
  rate: {fontSize: 32, fontWeight: '600', fontVariant: ['tabular-nums']}, rateUnit: {fontSize: 14, fontWeight: '400'},
  total: {alignItems: 'flex-end'}, totalNumber: {fontSize: 19, fontVariant: ['tabular-nums']},
  target: {borderTopWidth: 1, paddingTop: 12, marginTop: 12}, targetTitle: {fontSize: 13, marginTop: 4},
  captureRow: {flexDirection: 'row', alignItems: 'center', gap: 8, marginTop: 8}, captureAction: {maxWidth: '65%'},
  input: {width: 62, height: 40, borderRadius: 9, borderWidth: 1, padding: 7, textAlign: 'center', fontSize: 16},
  slider: {height: 42, marginTop: 8, borderWidth: 2, borderRadius: 8, justifyContent: 'center', paddingHorizontal: 12},
  track: {height: 5, borderRadius: 3}, fill: {height: 5, borderRadius: 3},
  thumb: {position: 'absolute', top: 6, width: 26, height: 26, borderRadius: 13, borderWidth: 3},
  sliderLabels: {flexDirection: 'row', justifyContent: 'space-between'}, sectionGap: {marginTop: 16},
  segments: {flexDirection: 'row', gap: 6, marginTop: 8}, advanced: {marginTop: 16},
  validation: {fontSize: 12, marginTop: 10}, error: {borderWidth: 1, padding: 14, borderRadius: 14},
  footer: {fontSize: 11, textAlign: 'center', lineHeight: 17},
});

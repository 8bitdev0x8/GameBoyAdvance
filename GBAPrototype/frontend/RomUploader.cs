using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;

sealed class Device {
    public string port {get;set;} public string description {get;set;} public string serial {get;set;}
    public int? vid {get;set;} public int? pid {get;set;}
    public bool supported {get;set;}
    public override string ToString() { return port + "  —  " + (supported ? "Feather RP2350 HSTX" : description); }
}

sealed class Card : Panel {
    public Card() { DoubleBuffered=true; BackColor=Color.FromArgb(24,33,49); }
    protected override void OnPaint(PaintEventArgs e) {
        base.OnPaint(e); e.Graphics.SmoothingMode=SmoothingMode.AntiAlias;
        using(var pen=new Pen(Color.FromArgb(42,55,75))) e.Graphics.DrawRectangle(pen,0,0,Width-1,Height-1);
    }
}

sealed class Uploader : Form {
    readonly Color bg=Color.FromArgb(12,19,31), fg=Color.FromArgb(239,244,252), muted=Color.FromArgb(154,170,193);
    readonly Color blue=Color.FromArgb(62,130,247), green=Color.FromArgb(71,211,164), red=Color.FromArgb(255,129,134);
    readonly JavaScriptSerializer json=new JavaScriptSerializer();
    readonly string root, script, python, stateFile;
    readonly Label romName=new Label(), romInfo=new Label(), boardInfo=new Label(), status=new Label(), resultInfo=new Label(), badge=new Label();
    readonly ComboBox ports=new ComboBox();
    readonly Button browse=new Button(), refresh=new Button(), buildFlash=new Button(), buildOnly=new Button(), flashLast=new Button(), cancel=new Button(), folder=new Button();
    readonly ProgressBar progress=new ProgressBar();
    readonly TextBox activity=new TextBox();
    readonly ToolTip tip=new ToolTip();
    readonly System.Windows.Forms.Timer scanTimer=new System.Windows.Forms.Timer();
    string rom="", artifact="", output="";
    bool busy, locked, scanning, closeAfterCancel;
    Process worker;

    public Uploader(string projectRoot) {
        root=projectRoot;script=Path.Combine(root,"GBAPrototype","tools","rom_uploader_backend.py");
        stateFile=Path.Combine(root,"build","rom-uploader","app-state.json");python=FindPython();
        Text="GBA ROM Uploader v"+Application.ProductVersion; BackColor=bg;ForeColor=fg;Font=new Font("Segoe UI",14,FontStyle.Regular,GraphicsUnit.Pixel);
        AutoScaleMode=AutoScaleMode.None;ClientSize=new Size(1000,810);MinimumSize=Size;StartPosition=FormStartPosition.CenterScreen;
        foreach(var l in new[]{romName,romInfo,boardInfo,status,resultInfo,badge})l.UseMnemonic=false;
        var mark=new Label {Text="GBA",Bounds=new Rectangle(28,28,62,46),BackColor=blue,ForeColor=Color.White,TextAlign=ContentAlignment.MiddleCenter,Font=new Font("Segoe UI",20,FontStyle.Bold,GraphicsUnit.Pixel)};Controls.Add(mark);
        AddLabel(this,"ROM Uploader",106,17,500,49,25,fg,true);
        AddLabel(this,"Choose a game. Send it to your Feather.",108,72,640,24,11,muted,false);
        badge.Bounds=new Rectangle(708,34,264,29);badge.TextAlign=ContentAlignment.MiddleRight;badge.ForeColor=muted;badge.Anchor=AnchorStyles.Top|AnchorStyles.Right;Controls.Add(badge);
        var game=AddCard(28,114,944,150);
        AddLabel(game,"01   CHOOSE YOUR GAME",24,17,650,22,10,muted,true);
        romName.Bounds=new Rectangle(24,52,710,33);romName.Font=new Font("Segoe UI",23,FontStyle.Bold,GraphicsUnit.Pixel);romName.AutoEllipsis=true;romName.ForeColor=fg;game.Controls.Add(romName);
        romInfo.Bounds=new Rectangle(26,99,860,31);romInfo.ForeColor=muted;romInfo.AutoEllipsis=true;game.Controls.Add(romInfo);
        StyleButton(browse,"Choose .gba file",752,49,164,43,false);browse.Anchor=AnchorStyles.Top|AnchorStyles.Right;game.Controls.Add(browse);browse.Click+=(s,e)=>ChooseRom();
        var board=AddCard(28,280,944,134);
        AddLabel(board,"02   SELECT YOUR FEATHER",24,16,650,22,10,muted,true);
        ports.Bounds=new Rectangle(24,53,710,34);ports.DropDownStyle=ComboBoxStyle.DropDownList;ports.Font=new Font("Segoe UI",15,FontStyle.Regular,GraphicsUnit.Pixel);ports.Anchor=AnchorStyles.Left|AnchorStyles.Top|AnchorStyles.Right;
        board.Controls.Add(ports);ports.SelectedIndexChanged+=(s,e)=>UpdateButtons();
        StyleButton(refresh,"Refresh ports",752,49,164,39,false);refresh.Anchor=AnchorStyles.Top|AnchorStyles.Right;board.Controls.Add(refresh);refresh.Click+=(s,e)=>RefreshPorts();
        boardInfo.Bounds=new Rectangle(25,98,870,24);boardInfo.ForeColor=muted;boardInfo.AutoEllipsis=true;board.Controls.Add(boardInfo);
        var flow=AddCard(28,430,944,111);
        AddLabel(flow,"03   BUILD & FLASH",24,15,650,22,10,muted,true);
        status.Bounds=new Rectangle(24,46,890,27);status.AutoEllipsis=true;status.ForeColor=fg;status.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;flow.Controls.Add(status);
        progress.Bounds=new Rectangle(25,85,890,7);progress.Style=ProgressBarStyle.Continuous;progress.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;flow.Controls.Add(progress);
        StyleButton(buildFlash,"Build & flash game",28,559,251,48,true);Controls.Add(buildFlash);buildFlash.Click+=(s,e)=>StartJob(true,false);
        StyleButton(buildOnly,"Build only",291,559,145,48,false);Controls.Add(buildOnly);buildOnly.Click+=(s,e)=>StartJob(false,false);
        StyleButton(flashLast,"Flash built game",448,559,181,48,false);Controls.Add(flashLast);flashLast.Click+=(s,e)=>StartJob(true,true);
        StyleButton(cancel,"Cancel",641,559,108,48,false);Controls.Add(cancel);cancel.Click+=(s,e)=>CancelJob();
        StyleButton(folder,"Open build folder",761,559,211,48,false);folder.Anchor=AnchorStyles.Top|AnchorStyles.Right;Controls.Add(folder);folder.Click+=(s,e)=> { if(Directory.Exists(output))Process.Start("explorer.exe",Quote(output)); };
        resultInfo.Bounds=new Rectangle(30,620,940,30);resultInfo.ForeColor=muted;resultInfo.AutoEllipsis=true;Controls.Add(resultInfo);
        AddLabel(this,"ACTIVITY",30,658,300,20,9,muted,true);
        activity.Bounds=new Rectangle(28,687,944,85);activity.Multiline=true;activity.ReadOnly=true;activity.ScrollBars=ScrollBars.Vertical;activity.BackColor=Color.FromArgb(8,14,24);activity.ForeColor=muted;activity.BorderStyle=BorderStyle.FixedSingle;activity.Font=new Font("Consolas",12,FontStyle.Regular,GraphicsUnit.Pixel);activity.Anchor=AnchorStyles.Top|AnchorStyles.Bottom|AnchorStyles.Left|AnchorStyles.Right;Controls.Add(activity);
        AddLabel(this,"Feather RP2350 HSTX  ·  8 MiB flash  ·  Local builds",30,782,940,20,9,muted,false).Anchor=AnchorStyles.Left|AnchorStyles.Bottom;
        LoadState();SetRom(rom);status.Text="Choose a game and the Feather's USB port to begin.";
        scanTimer.Interval=5000;scanTimer.Tick+=(s,e)=> { if(!busy)RefreshPorts(); };scanTimer.Start();
        Shown+=(s,e)=>RefreshPorts();FormClosing+=ClosingApp;UpdateButtons();
    }
    Card AddCard(int x,int y,int w,int h) {var p=new Card{Bounds=new Rectangle(x,y,w,h),Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right};Controls.Add(p);return p;}
    Label AddLabel(Control parent,string text,int x,int y,int w,int h,float size,Color color,bool bold) {var l=new Label {UseMnemonic=false,Text=text,Bounds=new Rectangle(x,y,w,h),ForeColor=color,Font=new Font("Segoe UI",size*1.3333f,bold?FontStyle.Bold:FontStyle.Regular,GraphicsUnit.Pixel)};parent.Controls.Add(l);return l;}
    void StyleButton(Button b,string text,int x,int y,int w,int h,bool primary) {b.UseMnemonic=false;b.Text=text;b.Bounds=new Rectangle(x,y,w,h);b.FlatStyle=FlatStyle.Flat;b.FlatAppearance.BorderColor=Color.FromArgb(54,70,94);b.BackColor=primary?blue:Color.FromArgb(27,39,57);b.ForeColor=Color.White;b.Font=new Font("Segoe UI",14,primary?FontStyle.Bold:FontStyle.Regular,GraphicsUnit.Pixel);b.Cursor=Cursors.Hand;b.UseVisualStyleBackColor=false;}
    string FindPython() {
        var custom=Environment.GetEnvironmentVariable("GBA_PYTHON");if(!String.IsNullOrEmpty(custom)&&File.Exists(custom))return custom;
        var home=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Programs","Python");
        if(Directory.Exists(home))foreach(var d in Directory.GetDirectories(home).OrderByDescending(p=>p)){var p=Path.Combine(d,"python.exe");if(File.Exists(p))return p;}
        return "python.exe";
    }
    public static string Quote(string value) {
        var b=new StringBuilder("\"");int n=0;foreach(char c in value){if(c=='\\'){++n;continue;}if(c=='\"'){b.Append('\\',n*2+1);b.Append(c);}else{b.Append('\\',n);b.Append(c);}n=0;}b.Append('\\',n*2);b.Append('"');return b.ToString();
    }
    ProcessStartInfo Command(IEnumerable<string> args) {return new ProcessStartInfo(python,String.Join(" ",new[]{script,"--frontend-version",Application.ProductVersion}.Concat(args).Select(Quote))) {UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true,RedirectStandardInput=true,WorkingDirectory=root,StandardOutputEncoding=Encoding.UTF8,StandardErrorEncoding=Encoding.UTF8};}
    void UI(Action action) {if(!IsDisposed&&IsHandleCreated)try{BeginInvoke(action);}catch(InvalidOperationException){}}
    void Log(string text) {if(activity.TextLength>100000)activity.Text=activity.Text.Substring(activity.TextLength-60000);activity.AppendText(text+Environment.NewLine);}
    static string S(Dictionary<string,object> d,string k){return d.ContainsKey(k)?Convert.ToString(d[k]):"";}
    static double N(Dictionary<string,object> d,string k){return d.ContainsKey(k)?Convert.ToDouble(d[k]):0;}
    static string MiB(double n){return (n/(1024*1024)).ToString("0.00")+" MiB";}
    void ChooseRom() {using(var d=new OpenFileDialog {Filter="Game Boy Advance game (*.gba)|*.gba",Title="Choose a Game Boy Advance ROM",CheckFileExists=true})if(d.ShowDialog(this)==DialogResult.OK){SetRom(d.FileName);SaveState();}}
    void SetRom(string path) {
        rom=path??"";if(!File.Exists(rom)){rom="";romName.Text="Select a Game Boy Advance ROM";romInfo.Text="One .gba game, stored on your Feather. Your ESP32 remains the controller bridge.";}
        else {var f=new FileInfo(rom);romName.Text=f.Name;romInfo.Text=MiB(f.Length)+" original game  ·  Repacked automatically before flashing";tip.SetToolTip(romName,rom);}
        UpdateButtons();
    }
    void RefreshPorts() {
        if(scanning||busy)return;scanning=true;refresh.Enabled=false;
        ThreadPool.QueueUserWorkItem(o=> {
            try {using(var p=new Process {StartInfo=Command(new[]{"--list-ports"})}) {p.Start();var text=p.StandardOutput.ReadToEnd();var err=p.StandardError.ReadToEnd();p.WaitForExit();if(p.ExitCode!=0)throw new Exception(err);var found=json.Deserialize<Device[]>(text);
                UI(()=> {var previous=ports.SelectedItem as Device;ports.Items.Clear();ports.Items.AddRange(found);var selected=found.FirstOrDefault(d=>previous!=null&&d.serial==previous.serial&&d.port==previous.port)??found.FirstOrDefault(d=>d.supported);
                    if(selected!=null)ports.SelectedItem=selected;else if(previous!=null&&previous.supported){ports.Items.Add(previous);ports.SelectedItem=previous;}UpdateButtons();});}}
            catch(Exception e){UI(()=>{badge.Text="USB discovery unavailable";Log(e.Message);});}
            finally{UI(()=>{scanning=false;refresh.Enabled=!busy;});}
        });
    }
    void UpdateButtons() {
        var device=ports.SelectedItem as Device;bool supported=device!=null&&device.supported;
        browse.Enabled=!busy;ports.Enabled=!busy;refresh.Enabled=!busy&&!scanning;
        buildFlash.Enabled=!busy&&rom!=""&&supported;buildOnly.Enabled=!busy&&rom!="";
        flashLast.Enabled=!busy&&File.Exists(artifact)&&supported;cancel.Enabled=busy&&!locked;folder.Enabled=Directory.Exists(output);
        boardInfo.Text=supported?"Feather selected on "+device.port+". Close Serial Monitor before uploading.":"Connect the Feather by USB. Select its port, not the ESP32 or a Bluetooth link.";
        boardInfo.ForeColor=supported?green:muted;badge.Text=supported?"●  Feather selected  ·  "+device.port:"○  Connect your Feather";badge.ForeColor=supported?green:muted;
    }
    void StartJob(bool flash,bool existing) {
        if(busy)return;var target=ports.SelectedItem as Device;if(flash&&(target==null||!target.supported))return;
        var args=new List<string>();if(existing){args.Add("--artifact");args.Add(artifact);}else{args.Add("--rom");args.Add(rom);}
        if(flash){args.AddRange(new[]{"--flash","--port",target.port,"--serial",target.serial});}
        busy=true;locked=false;progress.Style=ProgressBarStyle.Marquee;status.ForeColor=fg;status.Text="Starting…";Log("\r\n"+DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss")+"  "+(existing?"Uploading built game":flash?"Building and uploading game":"Building game"));UpdateButtons();
        ThreadPool.QueueUserWorkItem(o=> {
            try {using(var p=new Process {StartInfo=Command(args)}) {worker=p;
                p.OutputDataReceived+=(s,e)=>{if(e.Data!=null){try{var data=json.Deserialize<Dictionary<string,object>>(e.Data);UI(()=>Event(data));}catch{UI(()=>Log(e.Data));}}};
                p.ErrorDataReceived+=(s,e)=>{if(e.Data!=null)UI(()=>Log(e.Data));};p.Start();p.BeginOutputReadLine();p.BeginErrorReadLine();p.WaitForExit();int code=p.ExitCode;
                UI(()=> {busy=false;locked=false;progress.Style=ProgressBarStyle.Continuous;progress.Value=code==0?100:0;if(code!=0&&status.Text=="Starting…"){status.Text="Could not start. Check the activity details.";status.ForeColor=red;}UpdateButtons();if(closeAfterCancel)Close();});}}
            catch(Exception e){UI(()=>{busy=false;locked=false;progress.Style=ProgressBarStyle.Continuous;status.Text="Could not start the uploader. See activity details.";status.ForeColor=red;Log(e.Message);UpdateButtons();});}
            finally{worker=null;}
        });
    }
    void Event(Dictionary<string,object> d) {
        var kind=S(d,"event");
        if(kind=="versions")Log("Frontend "+S(d,"frontend")+" · Backend "+S(d,"backend")+" · Protocol "+S(d,"protocol"));
        else if(kind=="log")Log(S(d,"text"));
        else if(kind=="stage"){status.Text=S(d,"text");locked=S(d,"name")=="reset"||S(d,"name")=="flash";UpdateButtons();Log(status.Text);}
        else if(kind=="workspace"){output=S(d,"output");UpdateButtons();}
        else if(kind=="packed")romInfo.Text=MiB(N(d,"original_bytes"))+" game  →  "+MiB(N(d,"bytes"))+" packed";
        else if(kind=="built"||kind=="done") {
            output=S(d,"output");artifact=Path.Combine(output,"build.json");var layout=(Dictionary<string,object>)d["layout"];
            var game=(Dictionary<string,object>)d["rom"];resultInfo.Text="Built "+S(game,"file")+"  ·  "+MiB(N(layout,"flash_used_bytes"))+" / 8 MiB flash";
            tip.SetToolTip(resultInfo,output);SaveState();UpdateButtons();
            if(kind=="done"){bool uploaded=d.ContainsKey("uploaded")&&Convert.ToBoolean(d["uploaded"]);status.Text=uploaded?"Game uploaded and verified. Your Feather is restarting.":"Build ready. Select your Feather and use ‘Flash built game’.";status.ForeColor=green;Log(status.Text);}
        }
        else if(kind=="error"){status.Text=d.ContainsKey("cancelled")&&Convert.ToBoolean(d["cancelled"])?"Build cancelled.":"Could not finish. See the activity details below.";status.ForeColor=red;Log(S(d,"text"));}
    }
    void CancelJob(){if(!busy||locked||worker==null)return;try{worker.StandardInput.WriteLine("cancel");worker.StandardInput.Flush();status.Text="Cancelling the build…";}catch(Exception e){Log(e.Message);}}
    void ClosingApp(object sender,FormClosingEventArgs e){if(!busy)return;e.Cancel=true;if(locked){MessageBox.Show(this,"The upload is finishing. Keep this window and USB connection open until it completes.","Upload in progress",MessageBoxButtons.OK,MessageBoxIcon.Information);}else{closeAfterCancel=true;CancelJob();}}
    void SaveState(){try{Directory.CreateDirectory(Path.GetDirectoryName(stateFile));File.WriteAllText(stateFile,json.Serialize(new {rom=rom,artifact=artifact,output=output}));}catch(Exception e){Log(e.Message);}}
    void LoadState(){try{if(File.Exists(stateFile)){var d=json.Deserialize<Dictionary<string,object>>(File.ReadAllText(stateFile));rom=S(d,"rom");artifact=S(d,"artifact");output=S(d,"output");}}catch{}}
}

static class Program {
    [DllImport("user32.dll")]static extern bool SetProcessDPIAware();
    [STAThread]static void Main(string[] args) {
        SetProcessDPIAware();Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        string root=AppDomain.CurrentDomain.BaseDirectory;
        if(!File.Exists(Path.Combine(root,"GBAPrototype","tools","rom_uploader_backend.py"))){MessageBox.Show("Keep this application beside the GBAPrototype folder.","GBA ROM Uploader");return;}
        Application.Run(new Uploader(root));
    }
}

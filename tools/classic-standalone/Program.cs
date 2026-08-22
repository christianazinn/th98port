using System.Diagnostics;
using System.IO.Compression;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

namespace Th03PoddPortStandalone;

internal static class Program
{
    private const string PayloadResource = "TH03.PoDDPort.Payload.zip";
    private const string ProductDirectory = "PoDD Port";
    // Retained only to move an existing private profile to the neutral product
    // identity. New files and diagnostics are never written below this name.
    private const string LegacyProductDirectory = "TH03 Classic Native Test";
    private const string ProductRootEnvironment =
        "TH03_PODD_PORT_PRODUCT_ROOT";
    private const string LegacyProductRootEnvironment =
        "TH03_PODD_PORT_LEGACY_PRODUCT_ROOT";
    private const string SmokeTicksEnvironment =
        "TH03_PODD_PORT_SMOKE_TICKS";
    private const string UserRootEnvironment =
        "TH03_PODD_PORT_USER_ROOT";
    private const string ReplayRootEnvironment =
        "TH03_PODD_PORT_REPLAY_ROOT";
    private const string DeduplicatedArchiveMarker =
        ".deduplicate-YUME2.DAT";

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int MessageBoxW(
        nint window,
        string text,
        string caption,
        uint type
    );

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CreateHardLinkW(
        string fileName,
        string existingFileName,
        nint securityAttributes
    );

    [STAThread]
    private static int Main(string[] commandLine)
    {
        try
        {
            return Run(commandLine);
        }
        catch(Exception exception)
        {
            var message = "PoDD Port could not start.\n\n" +
                exception.Message;
            if(IsSmokeMode())
            {
                Console.Error.WriteLine(message);
            }
            else
            {
                ShowError(message);
            }
            return 1;
        }
    }

    private static int Run(IReadOnlyList<string> commandLine)
    {
        var localApplicationData = Environment.GetFolderPath(
            Environment.SpecialFolder.LocalApplicationData
        );
        if(string.IsNullOrWhiteSpace(localApplicationData))
        {
            throw new InvalidOperationException(
                "Windows did not provide a LocalApplicationData directory."
            );
        }

        var productRootOverride = Environment.GetEnvironmentVariable(
            ProductRootEnvironment
        );
        var productRoot = string.IsNullOrWhiteSpace(productRootOverride) ?
            Path.Combine(localApplicationData, ProductDirectory) :
            Path.GetFullPath(productRootOverride);
        var legacyProductRootOverride = Environment.GetEnvironmentVariable(
            LegacyProductRootEnvironment
        );
        if(!string.IsNullOrWhiteSpace(legacyProductRootOverride))
        {
            MigrateLegacyProductRoot(
                Path.GetFullPath(legacyProductRootOverride), productRoot
            );
        }
        else if(string.IsNullOrWhiteSpace(productRootOverride))
        {
            MigrateLegacyProductRoot(
                Path.Combine(localApplicationData, LegacyProductDirectory),
                productRoot
            );
        }
        var cacheRoot = Path.Combine(productRoot, "cache");
        var userRootOverride = Environment.GetEnvironmentVariable(
            UserRootEnvironment
        );
        var userRoot = string.IsNullOrWhiteSpace(userRootOverride) ?
            Path.Combine(productRoot, "user") :
            Path.GetFullPath(userRootOverride);
        Directory.CreateDirectory(cacheRoot);
        Directory.CreateDirectory(userRoot);
        var executable = Environment.ProcessPath ??
            throw new InvalidOperationException(
                "Windows did not provide the standalone executable path."
            );
        var executableDirectory = Path.GetDirectoryName(executable) ??
            throw new InvalidOperationException(
                "The standalone executable has no parent directory."
            );
        var replayRoot = SelectReplayRoot(executableDirectory, userRoot);

        using var payload = Assembly.GetExecutingAssembly().
            GetManifestResourceStream(PayloadResource) ??
            throw new InvalidOperationException(
                "The embedded TH03 payload is missing."
            );
        var payloadHash = Convert.ToHexString(SHA256.HashData(payload));
        if(!payload.CanSeek)
        {
            throw new InvalidOperationException(
                "The embedded TH03 payload cannot be rewound."
            );
        }
        payload.Position = 0;

        var installRoot = Path.Combine(cacheRoot, payloadHash);
        EnsurePayload(cacheRoot, installRoot, payloadHash, payload);
        PruneCache(cacheRoot, installRoot);

        var application = Path.Combine(installRoot, "app", "th03-app.exe");
        var bootstrap = Path.Combine(
            installRoot,
            "app",
            "podd-bootstrap.exe"
        );
        var updateRoot = Path.Combine(productRoot, "update");
        var updateState = Path.Combine(updateRoot, "PODD.UPDATE");
        var updateGenerations = Path.Combine(updateRoot, "generations");
        Directory.CreateDirectory(updateGenerations);
        var content = Path.Combine(installRoot, "content");
        var directNetplay = commandLine.Any(argument =>
            argument == "--netplay-host" || argument == "--netplay-join"
        );
        var dopamineArrange = commandLine.Count == 2 &&
            commandLine[0] == "--arrange" &&
            commandLine[1] == "dopamine-test-v1";
        if(commandLine.Count != 0 && !directNetplay && !dopamineArrange)
        {
            throw new InvalidOperationException(
                "Standalone command-line options currently require " +
                "--netplay-host, --netplay-join, or " +
                "--arrange dopamine-test-v1."
            );
        }
        if(directNetplay)
        {
            ValidateNetplayArguments(commandLine);
        }
        var arguments = directNetplay ?
            new List<string>
            {
                "--vs",
                "--data", content,
                "--main", Path.Combine(content, "MAIN.EXE"),
                "--font", Path.Combine(content, "FONT.ROM"),
                "--replay-root", replayRoot,
            } :
            new List<string>
            {
                dopamineArrange ? "--new-classic" : "--shell",
                "--data", content,
                "--main", Path.Combine(content, "MAIN.EXE"),
                "--mainl", Path.Combine(content, "MAINL.EXE"),
                "--op", Path.Combine(content, "OP.EXE"),
                "--font", Path.Combine(content, "FONT.ROM"),
                "--config", Path.Combine(userRoot, "YUME.CFG"),
                "--score", Path.Combine(userRoot, "YUME.NEM"),
                "--replay-root", replayRoot,
            };
        if(directNetplay || dopamineArrange)
        {
            arguments.AddRange(commandLine);
        }
        var languageOverlay = Path.Combine(content, "TH3EN.DAT");
        if(!directNetplay && File.Exists(languageOverlay))
        {
            arguments.Add("--language-overlay");
            arguments.Add(languageOverlay);
            var cutsceneText = Path.Combine(
                content, "cutscene-text-en-US.t3ctp"
            );
            if(File.Exists(cutsceneText))
            {
                arguments.Add("--cutscene-text-pack");
                arguments.Add(cutsceneText);
            }
        }
        var rhythmRom = Path.Combine(content, "YM2608.ROM");
        if(!directNetplay && File.Exists(rhythmRom))
        {
            arguments.Add("--rhythm-rom");
            arguments.Add(rhythmRom);
        }
        var rhythmPcm = Path.Combine(content, "T98RHYTHM.PCM");
        if(!directNetplay && File.Exists(rhythmPcm))
        {
            arguments.Add("--rhythm-pcm");
            arguments.Add(rhythmPcm);
        }
        var smokeTicks = Environment.GetEnvironmentVariable(
            SmokeTicksEnvironment
        );
        if(!directNetplay && !string.IsNullOrWhiteSpace(smokeTicks))
        {
            arguments.Add("--smoke-ticks");
            arguments.Add(smokeTicks);
        }
        EnsurePersistenceOutsideCache(
            installRoot,
            Path.Combine(userRoot, "YUME.CFG"),
            Path.Combine(userRoot, "YUME.NEM"),
            replayRoot
        );

        var start = new ProcessStartInfo
        {
            FileName = bootstrap,
            WorkingDirectory = installRoot,
            UseShellExecute = false,
            CreateNoWindow = true,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
        };
        start.ArgumentList.Add("--state");
        start.ArgumentList.Add(updateState);
        start.ArgumentList.Add("--generations");
        start.ArgumentList.Add(updateGenerations);
        start.ArgumentList.Add("--legacy");
        start.ArgumentList.Add(application);
        start.ArgumentList.Add("--working-directory");
        start.ArgumentList.Add(installRoot);
        start.ArgumentList.Add("--");
        foreach(var argument in arguments)
        {
            start.ArgumentList.Add(argument);
        }

        using var process = Process.Start(start) ??
            throw new InvalidOperationException(
                "Windows refused to start the native TH03 executable."
            );
        var standardOutput = process.StandardOutput.ReadToEndAsync();
        var standardError = process.StandardError.ReadToEndAsync();
        process.WaitForExit();
        Task.WaitAll(standardOutput, standardError);

        var log = new StringBuilder();
        log.AppendLine($"exit-code={process.ExitCode}");
        log.AppendLine($"replay-root={replayRoot}");
        log.AppendLine($"install-root={installRoot}");
        log.Append(standardOutput.Result);
        log.Append(standardError.Result);
        var logPath = Path.Combine(userRoot, "last-run.log");
        File.WriteAllText(logPath, log.ToString(), Encoding.UTF8);
        if(process.ExitCode != 0)
        {
            var message = $"PoDD Port exited with code {process.ExitCode}.\n\n" +
                $"Diagnostic log:\n{logPath}";
            if(IsSmokeMode())
            {
                Console.Error.WriteLine(message);
            }
            else
            {
                ShowError(message);
            }
        }
        return process.ExitCode;
    }

    private static void ValidateNetplayArguments(
        IReadOnlyList<string> commandLine
    )
    {
        var hostCount = 0;
        var joinCount = 0;
        var optionsWithValues = new HashSet<string>(StringComparer.Ordinal)
        {
            "--netplay-host",
            "--netplay-join",
            "--netplay-input-delay",
            "--p1",
            "--p2",
            "--rank",
            "--seed",
            "--tick-rate",
            "--smoke-ticks",
            "--netplay-replay-output",
        };
        for(var index = 0; index < commandLine.Count; index += 2)
        {
            var option = commandLine[index];
            if(!optionsWithValues.Contains(option) ||
                index + 1 >= commandLine.Count ||
                string.IsNullOrWhiteSpace(commandLine[index + 1]))
            {
                throw new InvalidOperationException(
                    $"Unsupported or incomplete netplay option: {option}"
                );
            }
            if(option == "--netplay-host")
            {
                hostCount++;
            }
            else if(option == "--netplay-join")
            {
                joinCount++;
            }
        }
        if(hostCount + joinCount != 1)
        {
            throw new InvalidOperationException(
                "Specify exactly one --netplay-host or --netplay-join option."
            );
        }
    }

    private static bool IsSmokeMode() => !string.IsNullOrWhiteSpace(
        Environment.GetEnvironmentVariable(SmokeTicksEnvironment)
    );

    private static void EnsurePersistenceOutsideCache(
        string installRoot,
        params string[] paths
    )
    {
        var prefix = Path.GetFullPath(installRoot).TrimEnd(
            Path.DirectorySeparatorChar,
            Path.AltDirectorySeparatorChar
        ) + Path.DirectorySeparatorChar;
        foreach(var path in paths)
        {
            if(Path.GetFullPath(path).StartsWith(
                prefix,
                StringComparison.OrdinalIgnoreCase
            ))
            {
                throw new InvalidOperationException(
                    "Persistent TH03 data must remain outside the payload cache."
                );
            }
        }
    }

    private static void MigrateLegacyProductRoot(
        string legacyRoot,
        string productRoot
    )
    {
        if(!Directory.Exists(legacyRoot))
        {
            return;
        }
        if(!Directory.Exists(productRoot))
        {
            Directory.Move(legacyRoot, productRoot);
            return;
        }

        // A packaging smoke may already have populated the new content cache.
        // Move the durable profile independently if no new profile exists yet.
        var legacyUserRoot = Path.Combine(legacyRoot, "user");
        var productUserRoot = Path.Combine(productRoot, "user");
        if(Directory.Exists(legacyUserRoot) && !Directory.Exists(productUserRoot))
        {
            Directory.Move(legacyUserRoot, productUserRoot);
        }
    }

    private static string SelectReplayRoot(
        string executableDirectory,
        string userRoot
    )
    {
        var requested = Environment.GetEnvironmentVariable(
            ReplayRootEnvironment
        );
        if(!string.IsNullOrWhiteSpace(requested))
        {
            var explicitRoot = Path.GetFullPath(requested);
            Directory.CreateDirectory(explicitRoot);
            return explicitRoot;
        }

        var portable = Path.Combine(executableDirectory, "REPLAY");
        if(CanWriteDirectory(portable))
        {
            return portable;
        }
        var fallback = Path.Combine(userRoot, "REPLAY");
        Directory.CreateDirectory(fallback);
        return fallback;
    }

    private static bool CanWriteDirectory(string directory)
    {
        try
        {
            Directory.CreateDirectory(directory);
            var probe = Path.Combine(
                directory,
                ".write-probe-" + Guid.NewGuid().ToString("N")
            );
            using var stream = new FileStream(
                probe,
                FileMode.CreateNew,
                FileAccess.Write,
                FileShare.None,
                1,
                FileOptions.DeleteOnClose
            );
            return true;
        }
        catch(UnauthorizedAccessException)
        {
            return false;
        }
        catch(IOException)
        {
            return false;
        }
        catch(NotSupportedException)
        {
            return false;
        }
    }

    private static void EnsurePayload(
        string cacheRoot,
        string installRoot,
        string payloadHash,
        Stream payload
    )
    {
        var marker = Path.Combine(installRoot, ".payload-sha256");
        if(IsComplete(marker, payloadHash))
        {
            return;
        }

        var lockPath = Path.Combine(cacheRoot, payloadHash + ".lock");
        using var lockFile = AcquireLock(lockPath);
        if(IsComplete(marker, payloadHash))
        {
            return;
        }
        if(Directory.Exists(installRoot))
        {
            Directory.Delete(installRoot, recursive: true);
        }

        var staging = Path.Combine(
            cacheRoot,
            ".extract-" + Guid.NewGuid().ToString("N")
        );
        Directory.CreateDirectory(staging);
        try
        {
            ExtractPayload(payload, staging);
            RestoreDeduplicatedArchive(staging);
            File.WriteAllText(
                Path.Combine(staging, ".payload-sha256"),
                payloadHash,
                Encoding.ASCII
            );
            Directory.Move(staging, installRoot);
        }
        finally
        {
            if(Directory.Exists(staging))
            {
                Directory.Delete(staging, recursive: true);
            }
        }
    }

    private static FileStream AcquireLock(string path)
    {
        for(var attempt = 0; attempt < 300; ++attempt)
        {
            try
            {
                return new FileStream(
                    path,
                    FileMode.OpenOrCreate,
                    FileAccess.ReadWrite,
                    FileShare.None
                );
            }
            catch(IOException) when(attempt != 299)
            {
                Thread.Sleep(100);
            }
        }
        throw new IOException("Timed out waiting for the payload cache lock.");
    }

    private static void PruneCache(string cacheRoot, string currentInstallRoot)
    {
        var current = Path.GetFullPath(currentInstallRoot);
        var generations = Directory.EnumerateDirectories(cacheRoot)
            .Where(path => IsPayloadHash(Path.GetFileName(path)))
            .Where(path => !string.Equals(
                Path.GetFullPath(path),
                current,
                StringComparison.OrdinalIgnoreCase
            ))
            .OrderByDescending(Directory.GetLastWriteTimeUtc)
            .ToArray();
        // Retain one prior generation so a recently launched older artifact is
        // unlikely to lose lazily loaded resources during a developer refresh.
        foreach(var generation in generations.Skip(1))
        {
            try
            {
                Directory.Delete(generation, recursive: true);
            }
            catch(IOException)
            {
            }
            catch(UnauthorizedAccessException)
            {
            }
        }
        foreach(var lockPath in Directory.EnumerateFiles(cacheRoot, "*.lock"))
        {
            try
            {
                File.Delete(lockPath);
            }
            catch(IOException)
            {
            }
            catch(UnauthorizedAccessException)
            {
            }
        }
    }

    private static bool IsPayloadHash(string value) =>
        value.Length == 64 && value.All(character =>
            character is >= '0' and <= '9' or >= 'A' and <= 'F'
        );

    private static bool IsComplete(string marker, string payloadHash)
    {
        try
        {
            return File.Exists(marker) &&
                File.ReadAllText(marker, Encoding.ASCII) == payloadHash &&
                File.Exists(Path.Combine(
                    Path.GetDirectoryName(marker)!, "app", "th03-app.exe"
                )) &&
                File.Exists(Path.Combine(
                    Path.GetDirectoryName(marker)!,
                    "app",
                    "podd-bootstrap.exe"
                )) &&
                File.Exists(Path.Combine(
                    Path.GetDirectoryName(marker)!, "content", "TOUHOU.DAT"
                )) &&
                Directory.Exists(Path.Combine(
                    Path.GetDirectoryName(marker)!, "content", "YUME2.DAT"
                ));
        }
        catch(IOException)
        {
            return false;
        }
    }

    private static void RestoreDeduplicatedArchive(string destinationRoot)
    {
        var marker = Path.Combine(destinationRoot, DeduplicatedArchiveMarker);
        if(!File.Exists(marker))
        {
            return;
        }
        var sourceRoot = Path.Combine(destinationRoot, "content", "YUME1.DAT");
        var aliasRoot = Path.Combine(destinationRoot, "content", "YUME2.DAT");
        if(!Directory.Exists(sourceRoot) || Directory.Exists(aliasRoot))
        {
            throw new InvalidDataException(
                "The deduplicated TH03 archive marker is inconsistent."
            );
        }
        Directory.CreateDirectory(aliasRoot);
        foreach(var source in Directory.EnumerateFiles(
            sourceRoot,
            "*",
            SearchOption.AllDirectories
        ))
        {
            var relative = Path.GetRelativePath(sourceRoot, source);
            var destination = Path.Combine(aliasRoot, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            // Hard links keep the extracted cache deduplicated on NTFS. A
            // checked byte copy is the compatibility fallback for other local
            // filesystems; the packaged executable remains deduplicated.
            if(!CreateHardLinkW(destination, source, 0))
            {
                File.Copy(source, destination, overwrite: false);
            }
        }
        File.Delete(marker);
    }

    private static void ExtractPayload(Stream payload, string destinationRoot)
    {
        using var archive = new ZipArchive(
            payload,
            ZipArchiveMode.Read,
            leaveOpen: true
        );
        var root = Path.GetFullPath(destinationRoot) +
            Path.DirectorySeparatorChar;
        foreach(var entry in archive.Entries)
        {
            var relative = entry.FullName.Replace(
                '/', Path.DirectorySeparatorChar
            );
            var destination = Path.GetFullPath(Path.Combine(root, relative));
            if(!destination.StartsWith(
                root,
                StringComparison.OrdinalIgnoreCase
            ))
            {
                throw new InvalidDataException(
                    "The embedded payload contains an invalid path."
                );
            }
            if(string.IsNullOrEmpty(entry.Name))
            {
                Directory.CreateDirectory(destination);
                continue;
            }
            Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
            entry.ExtractToFile(destination, overwrite: false);
        }
    }

    private static void ShowError(string message)
    {
        const uint errorIcon = 0x10U;
        _ = MessageBoxW(
            0,
            message,
            "PoDD Port",
            errorIcon
        );
    }
}
